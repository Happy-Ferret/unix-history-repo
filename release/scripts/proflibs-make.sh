#!/bin/sh

# Move the profiled libraries out to their own dist
for i in ${RD}/trees/bin/usr/lib/${OBJFORMAT}/*_p.a; do
	if [ -f $i ]; then
		mv $i ${RD}/trees/proflibs/usr/lib/${OBJFORMAT};
	fi;
done
