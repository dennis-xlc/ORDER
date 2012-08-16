#!/bin/bash - 
#===============================================================================
#
#          FILE:  remove_all_subversion_info.sh
# 
#         USAGE:  ./remove_all_subversion_info.sh 
# 
#   DESCRIPTION:  
# 
#       OPTIONS:  ---
#  REQUIREMENTS:  ---
#          BUGS:  ---
#         NOTES:  ---
#        AUTHOR:  (), 
#       COMPANY: 
#       CREATED: 08/16/2012 03:11:46 PM CST
#      REVISION:  ---
#===============================================================================

set -o nounset                              # Treat unset variables as an error

#!/bin/bash
#########################################################################
# Author: gnuhpc(http://blog.csdn.net/gnuhpc)
# Created Time: Thu 16 Aug 2012 03:11:46 PM CST
# File Name: remove_all_subversion_info.sh
# Description: 
#########################################################################


for path in `find -name .svn`; do

	echo $path
	rm $path -rf

done
