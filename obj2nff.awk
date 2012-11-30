# Takes a Wavefront obj file and turns it into nff format
# (currently translates only the Viewpoint .obj type files)
# Outputs groups together, marking each group with a "# group foo" comment
#
# Eric Haines, 3D/Eye Inc   erich@eye.com
# v1.0, 11/10/92

cat $* | awk '
{
    if ( $1 == "#" ) {
	print $0
    } else if ( $1 == "$" ) {
	print "#"$0
    } else if ( $1 == "g" ) {
	grp = $2
	for ( i = 3 ; i <= NF ; i++ ) {
	    grp = grp" "$i
	}
	groups[grp] = grp
    } else if ( $1 == "v" ) {
	nv++
	v[nv] = $2" "$3" "$4
    } else if ( $1 == "vn" ) {
	nvn++
	vn[nvn] = $2" "$3" "$4
#    } else if ( $1 == "vt" ) {
#	nvt++
#	vt[nv] = $2" "$3" "$4
#    } else if ( $1 == "s" ) {
#	str = $2
#	for ( i = 3 ; i <= NF ; i++ ) {
#	    str = str" "$i
#	}
#	printf( "# smoothing group %s\n", str )
    } else if ( $1 == "f" ) {
	if ( NF > 1 ) {
	    if ( index($2,"//") ) {
		ng = ++ngrp[grp]
		sg[grp,ng] = NF-1
		for ( i = 2 ; i <= NF ; i++ ) {
		    delim = index($i,"//")
		    n1 = substr($i,1,delim-1)
		    n2 = substr($i,delim+2,length($i)-delim-1)
		    j = i-1
		    sv[grp,ng,j] = sprintf( "%s %s",v[n1],vn[n2] )
		}
	    } else {
	printf("sorry, do not recognize the format %s yet - teach me!\n",$0 )
		exit
	    }
	}
    }
}
END {
    for ( typ in groups ) {
	if ( ngrp[typ] > 0 ) {
	    printf( "# group %s\n", typ )
	    for ( ng = 1 ; ng <= ngrp[typ] ; ng++ ) {
		printf ("pp %d\n", sg[typ,ng] )
		numv = sg[typ,ng]
		for ( i = 1 ; i <= numv ; i++ ) {
		    print sv[typ,ng,i]
		}
		np++
	    }
	}
    }
    printf( "# %d vertices, %d normals\n", nv, nvn )
    printf( "# %d polygons\n", np )
}'


