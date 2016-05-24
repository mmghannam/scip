/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
/*                                                                           */
/*        This file is part of the program PolySCIP                          */
/*                                                                           */
/*    Copyright (C) 2012-2016 Konrad-Zuse-Zentrum                            */
/*                            fuer Informationstechnik Berlin                */
/*                                                                           */
/*  PolySCIP is distributed under the terms of the ZIB Academic License.     */
/*                                                                           */
/*  You should have received a copy of the ZIB Academic License              */
/*  along with PolySCIP; see the file LICENCE.                               */
/*                                                                           */
/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#include "polyscip.h"

#include <algorithm> //std::transform, std::max
#include <cmath> //std::abs
#include <fstream>
#include <functional> //std::plus
#include <iostream>
#include <limits>
#include <ostream>
#include <memory> //std::addressof
#include <numeric> //std::inner_product
#include <stdexcept>
#include <string>
#include <tuple> //std::get
#include <utility> //std::make_pair
#include <vector>

#include "objscip/objscip.h"
#include "objscip/objscipdefplugins.h"
#include "cmd_line_args.h"
#include "global_functions.h"
#include "polyscip_types.h"
#include "prob_data_objectives.h"
#include "ReaderMOP.h"
#include "weight_space_polyhedron.h"

using std::addressof;
using std::cout;
using std::get;
using std::ostream;
using std::string;
using std::vector;
using polyscip::OutcomeType;
using polyscip::ValueType;

namespace polyscip {

    Polyscip::Polyscip(int argc, const char *const *argv)
            : cmd_line_args_(argc, argv),
              polyscip_status_(PolyscipStatus::Unsolved),
              scip_(nullptr),
              obj_sense_(SCIP_OBJSENSE_MINIMIZE), // default objective sense is minimization
              no_objs_(0),
              clock_total_(nullptr)
    {
        if (cmd_line_args_.hasTimeLimit() && cmd_line_args_.getTimeLimit() <= 0)
            throw std::domain_error("Invalid time limit.");
        if (cmd_line_args_.hasParameterFile() && !filenameIsOkay(cmd_line_args_.getParameterFile()))
            throw std::invalid_argument("Invalid parameter settings file.");
        if (!filenameIsOkay(cmd_line_args_.getProblemFile()))
            throw std::invalid_argument("Invalid problem file.");

        SCIPcreate(&scip_);
        assert (scip_ != nullptr);
        SCIPincludeDefaultPlugins(scip_);
        SCIPincludeObjReader(scip_, new ReaderMOP(scip_), TRUE);
        SCIPcreateClock(scip_, addressof(clock_total_));
        if (cmd_line_args_.hasParameterFile())
            SCIPreadParams(scip_, cmd_line_args_.getParameterFile().c_str());
    }

    Polyscip::~Polyscip() {
        SCIPfreeClock(scip_, addressof(clock_total_));
        SCIPfree(addressof(scip_));
    }


    SCIP_RETCODE Polyscip::computeNondomPoints() {
        SCIP_CALL( SCIPstartClock(scip_, clock_total_) );
        SCIP_CALL( computeSupported() );
        if (!cmd_line_args_.withUnsupported())
            std::cerr << "NOT IMPLEMENTED.\n";
        SCIP_CALL( SCIPstopClock(scip_, clock_total_) );
        return SCIP_OKAY;
    }

    SCIP_RETCODE Polyscip::initWeightSpace() {
        std::size_t obj_counter = 0;
        auto weight = WeightType(no_objs_,0.);
        while (polyscip_status_ == PolyscipStatus::Unsolved) {
            weight[obj_counter] = 1.;
            SCIP_CALL( setWeightedObjective(weight) );
            SCIP_CALL( solve() );
            auto scip_status = SCIPgetStatus(scip_);
            if (scip_status == SCIP_STATUS_INFORUNBD)
                scip_status = separateINFORUNBD(weight);
            weight[obj_counter] = 0.;
            ++obj_counter;
            SCIP_CALL( handleStatusInitPhase(scip_status, obj_counter) );
        }
        if (polyscip_status_ == PolyscipStatus::InitPhase) { // bounded outcome was found
            assert (!supported_.empty());
            weight_space_poly_ = global::make_unique<WeightSpacePolyhedron>(no_objs_, supported_.front().second);
            for (std::size_t i=0; i<unbounded_.size(); ++i) { // incorporate computed non-dominated rays
                auto wsp_changed = weight_space_poly_->updateInitialWSP(scip_, i, unbounded_[i].second, true);
                if (!wsp_changed)
                    throw std::runtime_error("No change in weight space polyhedron despite unbounded ray.\n");
            }

            while (obj_counter < no_objs_) {
                weight[obj_counter] = 1.;
                SCIP_CALL( setWeightedObjective(weight) );
                SCIP_CALL( solve() );
                auto scip_status = SCIPgetStatus(scip_);
                if (scip_status == SCIP_STATUS_INFORUNBD)
                    scip_status = separateINFORUNBD(weight);
                weight[obj_counter] = 0.;
                //todo nice way to incorporate unmarked vertices

                if (polyscip_status_ != PolyscipStatus::InitPhase)
                    return SCIP_OKAY;
                ++obj_counter;
            }
            weight_space_poly_->addCliqueEdgesToSkeleton();
            polyscip_status_ = PolyscipStatus::WeightSpacePhase;
        }
        return SCIP_OKAY;
    }

    SCIP_STATUS Polyscip::separateINFORUNBD(const WeightType& weight, bool with_presolving) {
        if (!with_presolving)
            SCIPsetPresolving(scip_, SCIP_PARAMSETTING_OFF, TRUE);
        auto zero_weight = WeightType(no_objs_, 0.);
        setWeightedObjective(zero_weight);
        solve(); // re-compute with zero objective
        if (!with_presolving)
            SCIPsetPresolving(scip_, SCIP_PARAMSETTING_DEFAULT, TRUE);
        auto status = SCIPgetStatus(scip_);
        setWeightedObjective(weight); // re-set to previous objective
        if (status == SCIP_STATUS_INFORUNBD) {
            if (with_presolving)
                separateINFORUNBD(weight, false);
            else
                throw std::runtime_error("INFORUNBD Status for problem with zero objective and no presolving.\n");
        }
        else if (status == SCIP_STATUS_UNBOUNDED) {
            throw std::runtime_error("UNBOUNDED Status for problem with zero objective.\n");
        }
        else if (status == SCIP_STATUS_OPTIMAL) { // previous problem was unbounded
            return SCIP_STATUS_UNBOUNDED;
        }
        return status;
    }


    SCIP_RETCODE Polyscip::handleStatusInitPhase(SCIP_STATUS status, bool& outcome_is_ray) {
        if (status == SCIP_STATUS_OPTIMAL) {
            SCIP_CALL( handleOptimalStatus() );
            outcome_is_ray = false;
        }
        else if (status == SCIP_STATUS_UNBOUNDED) {
            SCIP_CALL( handleUnboundedStatus() );
            outcome_is_ray = true;
        }
        else {
            SCIP_CALL( handleNonOptNonUnbdStatus(status) );
        }
        return SCIP_OKAY;
    }

    SCIP_RETCODE Polyscip::handleStatusInitPhase(SCIP_STATUS status, std::size_t obj_count) {
        if (status == SCIP_STATUS_OPTIMAL) {
            SCIP_CALL( handleOptimalStatus() );
            polyscip_status_ = PolyscipStatus::InitPhase;
        }
        else if (status == SCIP_STATUS_UNBOUNDED) {
            SCIP_CALL( handleUnboundedStatus() );
            if (obj_count >= no_objs_)
                polyscip_status_ = PolyscipStatus::Finished;
        }
        else {
            SCIP_CALL( handleNonOptNonUnbdStatus(status) );
        }
        return SCIP_OKAY;
    }

    SCIP_RETCODE Polyscip::handleNonOptNonUnbdStatus(SCIP_STATUS status) {
        assert (status != SCIP_STATUS_OPTIMAL && status != SCIP_STATUS_UNBOUNDED);
        if (status == SCIP_STATUS_INFORUNBD) {
            throw std::runtime_error("INFORUNBD Status unexpected at this stage.\n");
        }
        else if (status == SCIP_STATUS_TIMELIMIT) {
            polyscip_status_ = PolyscipStatus::TimeLimitReached;
        }
        else {
            polyscip_status_ = PolyscipStatus::Finished;
        }
        return SCIP_OKAY;
    }

    SCIP_RETCODE Polyscip::handleUnboundedStatus() {
        if (!SCIPhasPrimalRay(scip_)) {
            SCIP_CALL( SCIPsetPresolving(scip_, SCIP_PARAMSETTING_OFF, TRUE) );
            if (SCIPisTransformed(scip_))
                SCIP_CALL( SCIPfreeTransform(scip_) );
            SCIP_CALL( solve() );
            SCIP_CALL( SCIPsetPresolving(scip_, SCIP_PARAMSETTING_DEFAULT, TRUE) );
            if (SCIPgetStatus(scip_) != SCIP_STATUS_UNBOUNDED)
                throw std::runtime_error("Status UNBOUNDED expected.\n");
            if (!SCIPhasPrimalRay(scip_))
                throw std::runtime_error("Existence of primal ray expected.\n");
        }
        addResult();
        return SCIP_OKAY;
    }

    SCIP_RETCODE Polyscip::handleOptimalStatus() {
        auto best_sol = SCIPgetBestSol(scip_);
        SCIP_SOL *finite_sol = nullptr;
        SCIP_Bool same_obj_val = FALSE;
        SCIP_CALL(SCIPcreateFiniteSolCopy(scip_, addressof(finite_sol), best_sol, addressof(same_obj_val)));
        if (!same_obj_val) {
            auto diff = std::abs(SCIPgetSolOrigObj(scip_, best_sol) -
                                 SCIPgetSolOrigObj(scip_, finite_sol));
            if (diff > 1.0e-5) {
                std::cerr << "absolute value difference after calling SCIPcreateFiniteSolCopy: " << diff << "\n";
                SCIP_CALL(SCIPfreeSol(scip_, addressof(finite_sol)));
                throw std::runtime_error("SCIPcreateFiniteSolCopy: unacceptable difference in objective values.");
            }
        }
        assert (finite_sol != nullptr);
        addResult(true, finite_sol);
        SCIP_CALL(SCIPfreeSol(scip_, addressof(finite_sol)));
        return SCIP_OKAY;
    }

    void Polyscip::addResult(bool outcome_is_bounded, SCIP_SOL* primal_sol) {
        SolType sol;
        auto outcome = OutcomeType(no_objs_,0.);
        auto no_vars = SCIPgetNOrigVars(scip_);
        auto vars = SCIPgetOrigVars(scip_);
        auto objs_probdata = dynamic_cast<ProbDataObjectives*>(SCIPgetObjProbData(scip_));
        for (auto i=0; i<no_vars; ++i) {
            auto var_sol_val = outcome_is_bounded ? SCIPgetSolVal(scip_, primal_sol, vars[i]) :
                               SCIPgetPrimalRayVal(scip_, vars[i]);

            if (!SCIPisZero(scip_, var_sol_val)) {
                sol.emplace_back(SCIPvarGetName(vars[i]), var_sol_val);
                auto var_obj_vals = OutcomeType(no_objs_,0.);
                for (decltype(no_objs_) j=0; j<no_objs_; ++j)
                    var_obj_vals[j] = objs_probdata->getObjVal(vars[i], j, var_sol_val);
                std::transform(begin(outcome), end(outcome),
                               begin(var_obj_vals),
                               begin(outcome),
                               std::plus<ValueType>());

            }

        }

        if (outcome_is_bounded)
            supported_.push_back({sol,outcome});
        else
            unbounded_.push_back({sol,outcome});
    }



    SCIP_RETCODE Polyscip::solve() {
        if (cmd_line_args_.hasTimeLimit()) { // set SCIP timelimit
            auto remaining_time = std::max(cmd_line_args_.getTimeLimit() -
                                           SCIPgetClockTime(scip_, clock_total_), 0.);
            SCIP_CALL(SCIPsetRealParam(scip_, "limits/time", remaining_time));
        }
        SCIP_CALL( SCIPsolve(scip_) );    // actual SCIP solver call
        return SCIP_OKAY;
    }

    SCIP_RETCODE Polyscip::setWeightedObjective(const WeightType& weight){
        if (SCIPisTransformed(scip_))
            SCIP_CALL( SCIPfreeTransform(scip_) );
        auto obj_probdata = dynamic_cast<ProbDataObjectives*>(SCIPgetObjProbData(scip_));
        assert (obj_probdata != nullptr);
        auto vars = SCIPgetOrigVars(scip_);
        auto no_vars = SCIPgetNOrigVars(scip_);
        for (auto i=0; i<no_vars; ++i) {
            auto val = obj_probdata->getWeightedObjVal(vars[i], weight);
            SCIP_CALL( SCIPchgVarObj(scip_, vars[i], val) );
        }
        return SCIP_OKAY;
    }

    SCIP_RETCODE Polyscip::computeSupported() {
        SCIP_CALL( initWeightSpace() );
        if (polyscip_status_ == PolyscipStatus::WeightSpacePhase) {
            while (weight_space_poly_->hasUntestedWeight()) {
                auto untested_weight = weight_space_poly_->getUntestedWeight();
                global::print(untested_weight, "\nTESTING WEIGHT: ");
                SCIP_CALL( setWeightedObjective(untested_weight) );
                SCIP_CALL( solve() );
                auto scip_status = SCIPgetStatus(scip_);
                if (scip_status == SCIP_STATUS_INFORUNBD)
                    scip_status = separateINFORUNBD(untested_weight);
                if (scip_status == SCIP_STATUS_OPTIMAL) {
                    auto new_wov = SCIPgetPrimalbound(scip_);
                    if (SCIPisLT(scip_, new_wov, weight_space_poly_->getUntestedVertexWOV(untested_weight))==TRUE) {
                        std::cout << " found better optimum\n";
                        SCIP_CALL( handleOptimalStatus() ); //adds bounded result to supported_
                        auto last_added_outcome = supported_.back().second; //was added by handleStatus
                        std::cout << "incorportating new outcome: ";
                        global::print(last_added_outcome, " ");
                        weight_space_poly_->incorporateNewOutcome(scip_, cmd_line_args_.withCompleteLoopForObsolete(),
                                                                  untested_weight, last_added_outcome);
                    }
                    else {
                        std::cout << " did not find better optimum\n";
                        weight_space_poly_->incorporateKnownOutcome(untested_weight);
                    }
                }
                else if (scip_status == SCIP_STATUS_UNBOUNDED) {
                    std::cout << "UNBOUNDED STATUS\n";
                    SCIP_CALL( handleUnboundedStatus() ); //adds unbounded result to unbounded_
                    auto last_added_outcome = unbounded_.back().second; //was added by handleStatus
                    weight_space_poly_->incorporateNewOutcome(scip_, cmd_line_args_.withCompleteLoopForObsolete(),
                                                              untested_weight, last_added_outcome, true);
                }
                else {
                    std::cout << "NON OPTIMAL AND NON-UNBOUNDED STATUS\n";
                    SCIP_CALL( handleNonOptNonUnbdStatus(scip_status) ); //polyscip_status_ is set to finished or time limit reached
                    break;
                }
            }
            //weight_space_poly_->printMarkedVertices();
            //weight_space_poly_->printObsoleteVertices();
            //weight_space_poly_->printUnmarkedVertices();
            polyscip_status_ = PolyscipStatus::CompUnsupportedPhase;
        }
        return SCIP_OKAY;
    }

    void Polyscip::printSupportedResults(ostream& os) {
        for (const auto& result : supported_) {
            printPoint(result.second, os);
            printSol(result.first, os);
            os << "\n";
        }
        for (const auto& result : unbounded_) {
            printRay(result.second, os);
            printSol(result.first, os);
            os << "\n";
        }
    }

    void Polyscip::printSol(const SolType& sol, ostream& os) {
        os << " Non-zero solution variables: ";
        for (const auto& elem : sol)
            os << elem.first << "=" << elem.second << " ";
    }

    void Polyscip::printPoint(const OutcomeType& point, ostream& os) {
        global::print(point, {"Point = "}, os);
    }

    void Polyscip::printRay(const OutcomeType& ray, ostream& os) {
        global::print(ray, {"Ray = "}, os);
    }

    bool Polyscip::filenameIsOkay(const string& filename) {
        std::ifstream file(filename.c_str());
        return file.good();
    }

    SCIP_RETCODE Polyscip::readProblem() {
        auto filename = cmd_line_args_.getProblemFile();
        SCIP_CALL( SCIPreadProb(scip_, filename.c_str(), "mop") );
        auto obj_probdata = dynamic_cast<ProbDataObjectives*>(SCIPgetObjProbData(scip_));
        assert (obj_probdata != nullptr);
        no_objs_ = obj_probdata->getNObjs();
        if (SCIPgetObjsense(scip_) == SCIP_OBJSENSE_MAXIMIZE) {
            obj_sense_ = SCIP_OBJSENSE_MAXIMIZE;
            // internally we treat problem as min problem and negate objective coefficients
            SCIPsetObjsense(scip_, SCIP_OBJSENSE_MINIMIZE);
            obj_probdata->negateAllCoeffs();
        }
        if (cmd_line_args_.beVerbose()) {
            cout << "No of objectives: " << no_objs_;
            cout << "\nObjective sense: ";
            if (obj_sense_ == SCIP_OBJSENSE_MAXIMIZE)
                cout << "MAXIMIZE\n";
            else
                cout << "MINIMIZE\n";
        }
        obj_probdata = nullptr;
        return SCIP_OKAY;
    }

}
