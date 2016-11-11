#!/usr/bin/env bash
#
# Run all tests of applications, arguments are passed to the make test command.
# Parameter "-q" turns on the quiet mode, i.e., does not output the logging of the programs.
#
#

APPLICATIONS=(Coloring Scheduler STP)
LPSOLVERS=(spx2)
OPTS=(dbg)

echo "Running all tests on applications."

# parse command line
MAKEARGS=""
QUIET=0
for i in $@
do
    if test "$i" = "-q"
    then
	echo "Quiet mode."
	QUIET=1
    else
	MAKEARGS="$MAKEARGS $i"
    fi
done


# determine architecture
ARCH=`uname -m | \
    sed \
    -e 's/sun../sparc/' \
    -e 's/i.86/x86/' \
    -e 's/i86pc/x86/' \
    -e 's/[0-9]86/x86/' \
    -e 's/amd64/x86_64/' \
    -e 's/IP../mips/' \
    -e 's/9000..../hppa/' \
    -e 's/Power\ Macintosh/ppc/' \
    -e 's/00........../pwr4/'`
OSTYPE=`uname -s | tr '[:upper:]' '[:lower:]' | \
    sed \
    -e 's/cygwin.*/cygwin/' \
    -e 's/irix../irix/' \
    -e 's/windows.*/windows/' \
    -e 's/mingw.*/mingw/'`

# prepare log file
echo "" > applicationtestsummary.log

for APPLICATION in ${APPLICATIONS[@]}
do
    echo
    echo
    echo ===== $APPLICATION =====
    echo ===== $APPLICATION ===== >> applicationtestsummary.log
    echo
    cd $APPLICATION
    for OPT in ${OPTS[@]}
    do
	for LPS in ${LPSOLVERS[@]}
	do
	    LPILIB=../../lib/liblpi$LPS.$OSTYPE.$ARCH.gnu.$OPT.a
	    if test -e $LPILIB
            then
		SCIPLIB=../../lib/libscip.$OSTYPE.$ARCH.gnu.$OPT.a
		if test -e $SCIPLIB
		then
		    echo make OPT=$OPT LPS=$LPS $MAKEARGS
		    if (! make OPT=$OPT LPS=$LPS $MAKEARGS )
		    then
			echo "Making "$APPLICATION" failed." >> ../applicationtestsummary.log
			exit $STATUS
		    else
			echo "Making "$APPLICATION" successful." >> ../applicationtestsummary.log
		    fi
		    echo
		    if test $QUIET = 1
		    then
			echo make OPT=$OPT LPS=$LPS $MAKEARGS test
			if ( ! make OPT=$OPT LPS=$LPS $MAKEARGS test > /dev/null )
			then
			    echo "Testing "$APPLICATION" failed."
			    echo "Testing "$APPLICATION" failed." >> ../applicationtestsummary.log
			    exit $STATUS
			fi
		    else
			echo make OPT=$OPT LPS=$LPS $MAKEARGS test
			if ( ! make OPT=$OPT LPS=$LPS $MAKEARGS test )
			then
			    echo "Testing "$APPLICATION" failed."
			    echo "Testing "$APPLICATION" failed." >> ../applicationtestsummary.log
			    exit $STATUS
			fi
		    fi
		    echo "Testing "$APPLICATION" successful."
		    echo "Testing "$APPLICATION" successful." >> ../applicationtestsummary.log

		    # find most recently changed result file and display it
		    if test -d check/results
		    then
			RESFILE=`find check/results/*.res -type f -printf '%T@ %p\n' | sort -n | tail -1 | cut -f2- -d" "`
			if test -e $RESFILE
			then
			    cat $RESFILE >> ../applicationtestsummary.log
			fi
		    fi
		else
		    echo $SCIPLIB" does not exist - skipping combination ("$OPT", "$LPS")" >> ../applicationtestsummary.log
		    echo $SCIPLIB" does not exist - skipping combination ("$OPT", "$LPS")"
		fi
            else
		echo $LPILIB" does not exist - skipping combination ("$OPT", "$LPS")" >> ../applicationtestsummary.log
		echo $LPILIB" does not exist - skipping combination ("$OPT", "$LPS")"
            fi
	    echo
	    echo >> ../applicationtestsummary.log
	done
    done
    cd - > /dev/null
done

echo
echo
echo ===== Summary =====

cat applicationtestsummary.log
rm -f applicationtestsummary.log
