#!/bin/bash

# For plcontainer CLI testing. Called in sql/test_utility.sql

set -x

plcontainer -h

######################## test image-add ###############################
plcontainer image-add
plcontainer image-add nonexist
plcontainer image-add -f nonexist_file | sed -e 's/.*ERROR]://' -e 's/.*INFO]://' -e 's/.*CRITICAL]://'
plcontainer image-add -u nonexist_url | sed -e 's/.*ERROR]://' -e 's/.*INFO]://' -e 's/.*CRITICAL]://'

######################## test image-delete ###############################
plcontainer image-delete
plcontainer image-delete nonexist
plcontainer image-delete -i nonexist_image | sed -e 's/.*ERROR]://' -e 's/.*INFO]://' -e 's/.*CRITICAL]://'

######################## test image-list ###############################
plcontainer image-list >/tmp/tmp.image-list.out # We just care about whether there is stderr.
plcontainer image-list not_exist

#Backup the runtime configurations.
plcontainer runtime-list >test_backup_cfg_file
cat <<EOF >tmp_cfg_file
<?xml version="1.0" ?>
<configuration>
</configuration>
EOF
plcontainer runtime-restore -f tmp_cfg_file \
	| sed -e 's/.*ERROR]://' -e 's/.*INFO]://' -e 's/.*CRITICAL]://' \
	| grep -v 'Distributing to'
# We do not want $GPHOME to be in exp output.
set +x
plcontainer runtime-list |  sed -e "s|${GPHOME}|GPHOME|"
set -x
rm -f tmp_cfg_file

######################## test runtime-add/delte/list ###############################
plcontainer runtime-add -r nonexist_runtime
plcontainer runtime-add -i nonexist_image
plcontainer runtime-add -r nonexist_runtime -i nonexist_image
plcontainer runtime-add -r nonexist_runtime -i nonexist_image -l java

plcontainer runtime-add -r runtime1 -i image1 -l python \
	| sed -e 's/.*ERROR]://' -e 's/.*INFO]://' -e 's/.*CRITICAL]://' \
	| grep -v 'Distributing to'
set +x
plcontainer runtime-list |  sed -e "s|${GPHOME}|GPHOME|"
set -x
plcontainer runtime-delete -r runtime1 | sed -e 's/.*ERROR]://' -e 's/.*INFO]://' -e 's/.*CRITICAL]://' \
	| grep -v 'Distributing to'
set +x
plcontainer runtime-list |  sed -e "s|${GPHOME}|GPHOME|"
set -x
plcontainer runtime-delete -r runtime1 | sed -e 's/.*ERROR]://' -e 's/.*INFO]://' -e 's/.*CRITICAL]://' \
	| grep -v 'Distributing to'

plcontainer runtime-add -r runtime1 -i image1 -l python -v /host_dir/shared:/container_dir/shared:rw \
	| sed -e 's/.*ERROR]://' -e 's/.*INFO]://' -e 's/.*CRITICAL]://' \
	| grep -v 'Distributing to'
plcontainer runtime-delete -r runtime1 \
	| sed -e 's/.*ERROR]://' -e 's/.*INFO]://' -e 's/.*CRITICAL]://' \
	| grep -v 'Distributing to'
plcontainer runtime-add -r runtime1 -i image1 -l python -v /host_dir1/shared:/container_dir1/shared:rw \
		-v /host_dir1/shared:/container_dir1/shared:ro \
	| sed -e 's/.*ERROR]://' -e 's/.*INFO]://' -e 's/.*CRITICAL]://' \
	| grep -v 'Distributing to'
plcontainer runtime-delete -r runtime1 \
	| sed -e 's/.*ERROR]://' -e 's/.*INFO]://' -e 's/.*CRITICAL]://' \
	| grep -v 'Distributing to'

plcontainer runtime-add -r runtime1 -i image1 -l python -v /host_dir1/shared1:/container_dir1/shared1:rw \
		-v /host_dir1/shared2:/container_dir1/shared2:ro \
	| sed -e 's/.*ERROR]://' -e 's/.*INFO]://' -e 's/.*CRITICAL]://' \
	| grep -v 'Distributing to'
plcontainer runtime-add -r runtime2 -i image2 -l r -v /host_dir2/shared1:/container_dir2/shared1:rw \
		-v /host_dir2/shared2:/container_dir2/shared2:ro -s memory_mb=512 -s user_network=yes \
	| sed -e 's/.*ERROR]://' -e 's/.*INFO]://' -e 's/.*CRITICAL]://' \
	| grep -v 'Distributing to'
set +x
plcontainer runtime-list |  sed -e "s|${GPHOME}|GPHOME|"
set -x
plcontainer runtime-delete -r runtime1 \
	| sed -e 's/.*ERROR]://' -e 's/.*INFO]://' -e 's/.*CRITICAL]://' \
	| grep -v 'Distributing to'
plcontainer runtime-delete -r runtime2 \
	| sed -e 's/.*ERROR]://' -e 's/.*INFO]://' -e 's/.*CRITICAL]://' \
	| grep -v 'Distributing to'
set +x
plcontainer runtime-list |  sed -e "s|${GPHOME}|GPHOME|"
set -x

#recover the origincal runtime configurations.
plcontainer runtime-restore -f test_backup_cfg_file \
	| sed -e 's/.*ERROR]://' -e 's/.*INFO]://' -e 's/.*CRITICAL]://' \
	| grep -v 'Distributing to'
# We do not remove test_backup_cfg_file for checking in case there are test errors.
