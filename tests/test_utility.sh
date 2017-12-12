#!/bin/bash

# For plcontainer CLI testing. Called in sql/test_utility.sql

plcontainer -h

######################## test image-add ###############################
echo "Test image-add: negative cases"
plcontainer image-add
plcontainer image-add nonexist
plcontainer image-add -f nonexist_file | sed -e 's/.*ERROR]://' -e 's/.*INFO]://' -e 's/.*CRITICAL]://'
plcontainer image-add -u nonexist_url | sed -e 's/.*ERROR]://' -e 's/.*INFO]://' -e 's/.*CRITICAL]://'

######################## test image-delete ###############################
echo "Test image-delete: negative cases"
plcontainer image-delete
plcontainer image-delete nonexist

######################## test image-list ###############################
echo "Test image-list: negative cases"
plcontainer image-list >/tmp/tmp.image-list.out # We just care about whether there is stderr.
plcontainer image-list not_exist

echo "Prepare a blank runtime configuration file and test runtime-list"
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
plcontainer runtime-list |  sed -e "s|${GPHOME}|GPHOME|"
rm -f tmp_cfg_file

######################## test runtime-add/delte/list ###############################
echo "Test runtime-add: nagative cases"
plcontainer runtime-add -r nonexist_runtime
plcontainer runtime-add -i nonexist_image
plcontainer runtime-add -r nonexist_runtime -i nonexist_image
plcontainer runtime-add -r nonexist_runtime -i nonexist_image -l java

echo "Test runtime-add, runtime-list, and runtime-delete"
plcontainer runtime-add -r runtime1 -i image1 -l python \
	| sed -e 's/.*ERROR]://' -e 's/.*INFO]://' -e 's/.*CRITICAL]://' \
	| grep -v 'Distributing to'
plcontainer runtime-list |  sed -e "s|${GPHOME}|GPHOME|"
plcontainer runtime-delete -r runtime1 | sed -e 's/.*ERROR]://' -e 's/.*INFO]://' -e 's/.*CRITICAL]://' \
	| grep -v 'Distributing to'
plcontainer runtime-list |  sed -e "s|${GPHOME}|GPHOME|"
plcontainer runtime-delete -r runtime1 | sed -e 's/.*ERROR]://' -e 's/.*INFO]://' -e 's/.*CRITICAL]://' \
	| grep -v 'Distributing to'

echo "Test runtime-add with shared directories"
plcontainer runtime-add -r runtime1 -i image1 -l python -v /host_dir/shared:/container_dir/shared:rw \
	| sed -e 's/.*ERROR]://' -e 's/.*INFO]://' -e 's/.*CRITICAL]://' \
	| grep -v 'Distributing to'
plcontainer runtime-delete -r runtime1 \
	| sed -e 's/.*ERROR]://' -e 's/.*INFO]://' -e 's/.*CRITICAL]://' \
	| grep -v 'Distributing to'
plcontainer runtime-add -r runtime1 -i image1 -l python -v /host_dir1/shared:/container_dir1/shared:rw \
		-v /host_dir2/shared:/container_dir2/shared \
	| sed -e 's/.*ERROR]://' -e 's/.*INFO]://' -e 's/.*CRITICAL]://' \
	| grep -v 'Distributing to'
plcontainer runtime-delete -r runtime1 \
	| sed -e 's/.*ERROR]://' -e 's/.*INFO]://' -e 's/.*CRITICAL]://' \
	| grep -v 'Distributing to'

echo "Test runtime-add with both shared directories and settings"
plcontainer runtime-add -r runtime1 -i image1 -l python -v /host_dir1/shared1:/container_dir1/shared1:rw \
		-v /host_dir1/shared2:/container_dir1/shared2:ro \
	| sed -e 's/.*ERROR]://' -e 's/.*INFO]://' -e 's/.*CRITICAL]://' \
	| grep -v 'Distributing to'
plcontainer runtime-add -r runtime2 -i image2 -l r -v /host_dir2/shared1:/container_dir2/shared1:rw \
		-v /host_dir2/shared2:/container_dir2/shared2:ro -s memory_mb=512 -s user_network=yes \
	| sed -e 's/.*ERROR]://' -e 's/.*INFO]://' -e 's/.*CRITICAL]://' \
	| grep -v 'Distributing to'
plcontainer runtime-list |  sed -e "s|${GPHOME}|GPHOME|"
plcontainer runtime-delete -r runtime1 \
	| sed -e 's/.*ERROR]://' -e 's/.*INFO]://' -e 's/.*CRITICAL]://' \
	| grep -v 'Distributing to'
plcontainer runtime-delete -r runtime2 \
	| sed -e 's/.*ERROR]://' -e 's/.*INFO]://' -e 's/.*CRITICAL]://' \
	| grep -v 'Distributing to'
plcontainer runtime-list |  sed -e "s|${GPHOME}|GPHOME|"

echo "Test runtime-replace: negative cases"
plcontainer runtime-replace -r runtime3
plcontainer runtime-replace -r runtime3 -i image3
plcontainer runtime-replace -r runtime3 -i image3 -l java
echo "Test runtime-replace: add a new one"
plcontainer runtime-replace -r runtime3 -i image2 -l r -v /host_dir3/shared1:/container_dir3/shared1:rw \
	-v /host_dir3/shared2:/container_dir3/shared2:ro -s memory_mb=512 -s user_network=yes \
	| sed -e 's/.*ERROR]://' -e 's/.*INFO]://' -e 's/.*CRITICAL]://' \
	| grep -v 'Distributing to'
plcontainer runtime-list |  sed -e "s|${GPHOME}|GPHOME|"
echo "Test runtime-replace: replace"
plcontainer runtime-replace -r runtime3 -i image2 -l r -v /host_dir3/shared3:/container_dir3/shared3:rw \
	-s user_network=no \
	| sed -e 's/.*ERROR]://' -e 's/.*INFO]://' -e 's/.*CRITICAL]://' \
	| grep -v 'Distributing to'
plcontainer runtime-list |  sed -e "s|${GPHOME}|GPHOME|"

#recover the origincal runtime configurations.
echo "Recover the previous runtime configuration file"
plcontainer runtime-restore -f test_backup_cfg_file \
	| sed -e 's/.*ERROR]://' -e 's/.*INFO]://' -e 's/.*CRITICAL]://' \
	| grep -v 'Distributing to'
# We do not remove test_backup_cfg_file for checking in case there are test errors.
