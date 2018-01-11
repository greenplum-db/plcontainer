#!/bin/bash
#------------------------------------------------------------------------------
#
#
# Copyright (c) 2017-Present Pivotal Software, Inc
#
#------------------------------------------------------------------------------
#
# For plcontainer CLI testing. Called in sql/test_utility.sql

plcontainer -h

######################## test image-add ###############################
echo
echo "Test image-add: negative cases"
plcontainer image-add
plcontainer image-add nonexist
plcontainer image-add -f nonexist_file \
	| sed -e 's/.*ERROR]://' -e 's/.*INFO]://' -e 's/.*CRITICAL]://' -e 's/.*WARNING]://'
plcontainer image-add -u nonexist_url \
	| sed -e 's/.*ERROR]://' -e 's/.*INFO]://' -e 's/.*CRITICAL]://' -e 's/.*WARNING]://'

######################## test image-delete ###############################
echo
echo "Test image-delete: negative cases"
plcontainer image-delete
plcontainer image-delete nonexist

######################## test image-list ###############################
echo
echo "Test image-list: negative cases"
plcontainer image-list >/tmp/tmp.image-list.out # We just care about whether there is stderr.
plcontainer image-list not_exist

echo
echo "Prepare a blank runtime configuration file and test runtime-backup"
#Backup the runtime configurations.
plcontainer runtime-backup -f /tmp/test_backup_cfg_file_test_utility
cat <<EOF >tmp_cfg_file
<?xml version="1.0" ?>
<configuration>
</configuration>
EOF
plcontainer runtime-restore -f tmp_cfg_file \
	| sed -e 's/.*ERROR]://' -e 's/.*INFO]://' -e 's/.*CRITICAL]://' -e 's/.*WARNING]://' \
	| grep -v 'Distributing to'
# We do not want $GPHOME to be in exp output.
plcontainer runtime-backup -f /tmp/backup_file; cat /tmp/backup_file |  sed -e "s|${GPHOME}|GPHOME|"
rm -f tmp_cfg_file

######################## test runtime-add/delte/show/backup ###############################
echo
echo "Test runtime-add: nagative cases"
plcontainer runtime-add -r nonexist_runtime
plcontainer runtime-add -i nonexist_image
plcontainer runtime-add -r nonexist_runtime -i nonexist_image
plcontainer runtime-add -r nonexist_runtime -i nonexist_image -l java

echo
echo "Test runtime-add, runtime-backup, and runtime-delete"
plcontainer runtime-add -r runtime1 -i image1 -l python \
	| sed -e 's/.*ERROR]://' -e 's/.*INFO]://' -e 's/.*CRITICAL]://' -e 's/.*WARNING]://' \
	| grep -v 'Distributing to'

plcontainer runtime-backup -f /tmp/backup_file; cat /tmp/backup_file |  sed -e "s|${GPHOME}|GPHOME|"
plcontainer runtime-delete -r runtime1 \
	| sed -e 's/.*ERROR]://' -e 's/.*INFO]://' -e 's/.*CRITICAL]://' -e 's/.*WARNING]://' \
	| grep -v 'Distributing to'
plcontainer runtime-backup -f /tmp/backup_file; cat /tmp/backup_file |  sed -e "s|${GPHOME}|GPHOME|"
plcontainer runtime-delete -r runtime1 \
	| sed -e 's/.*ERROR]://' -e 's/.*INFO]://' -e 's/.*CRITICAL]://' -e 's/.*WARNING]://' \
	| grep -v 'Distributing to'

echo
echo "Test runtime-add with shared directories"
plcontainer runtime-add -r runtime1 -i image1 -l python -v /host_dir/shared:/container_dir/shared:rw \
	| sed -e 's/.*ERROR]://' -e 's/.*INFO]://' -e 's/.*CRITICAL]://' -e 's/.*WARNING]://' \
	| grep -v 'Distributing to'
plcontainer runtime-delete -r runtime1 \
	| sed -e 's/.*ERROR]://' -e 's/.*INFO]://' -e 's/.*CRITICAL]://' -e 's/.*WARNING]://' \
	| grep -v 'Distributing to'
plcontainer runtime-add -r runtime1 -i image1 -l python -v /host_dir1/shared:/container_dir1/shared:rw \
		-v /host_dir2/shared:/container_dir2/shared \
	| sed -e 's/.*ERROR]://' -e 's/.*INFO]://' -e 's/.*CRITICAL]://' -e 's/.*WARNING]://' \
	| grep -v 'Distributing to'
plcontainer runtime-delete -r runtime1 \
	| sed -e 's/.*ERROR]://' -e 's/.*INFO]://' -e 's/.*CRITICAL]://' -e 's/.*WARNING]://' \
	| grep -v 'Distributing to'

echo
echo "Test runtime-add with both shared directories and settings"
plcontainer runtime-add -r runtime1 -i image1 -l python -v /host_dir1/shared1:/container_dir1/shared1:rw \
		-v /host_dir1/shared2:/container_dir1/shared2:ro \
	| sed -e 's/.*ERROR]://' -e 's/.*INFO]://' -e 's/.*CRITICAL]://' -e 's/.*WARNING]://' \
	| grep -v 'Distributing to'
plcontainer runtime-add -r runtime2 -i image2 -l r -v /host_dir2/shared1:/container_dir2/shared1:rw \
		-v /host_dir2/shared2:/container_dir2/shared2:ro -s memory_mb=512 -s use_container_network=yes \
	| sed -e 's/.*ERROR]://' -e 's/.*INFO]://' -e 's/.*CRITICAL]://' -e 's/.*WARNING]://' \
	| grep -v 'Distributing to'
plcontainer runtime-show -r runtime_not_exist \
	| sed -e 's/.*ERROR]://' -e 's/.*INFO]://' -e 's/.*CRITICAL]://' -e 's/.*WARNING]://'
plcontainer runtime-show -r runtime_not_exist >/dev/null; echo $?
plcontainer runtime-show |  sed -e "s|${GPHOME}|GPHOME|"
plcontainer runtime-show -r runtime1 |  sed -e "s|${GPHOME}|GPHOME|"
plcontainer runtime-delete -r runtime1 \
	| sed -e 's/.*ERROR]://' -e 's/.*INFO]://' -e 's/.*CRITICAL]://' -e 's/.*WARNING]://' \
	| grep -v 'Distributing to'
plcontainer runtime-delete -r runtime2 \
	| sed -e 's/.*ERROR]://' -e 's/.*INFO]://' -e 's/.*CRITICAL]://' -e 's/.*WARNING]://' \
	| grep -v 'Distributing to'
plcontainer runtime-show |  sed -e "s|${GPHOME}|GPHOME|" \
	| sed -e 's/.*ERROR]://' -e 's/.*INFO]://' -e 's/.*CRITICAL]://' -e 's/.*WARNING]://'

echo
echo "Test runtime-replace: negative cases"
plcontainer runtime-replace -r runtime3
plcontainer runtime-replace -r runtime3 -i image3
plcontainer runtime-replace -r runtime3 -i image3 -l java
plcontainer runtime-replace -r runtime3 -i image3 -l r -s user_network=yes \
	| sed -e 's/.*ERROR]://' -e 's/.*INFO]://' -e 's/.*CRITICAL]://' -e 's/.*WARNING]://'

echo
echo "Test runtime-replace: add a new one"
plcontainer runtime-replace -r runtime3 -i image2 -l r -v /host_dir3/shared1:/container_dir3/shared1:rw \
	-v /host_dir3/shared2:/container_dir3/shared2:ro -s memory_mb=512 -s use_container_network=yes \
	| sed -e 's/.*ERROR]://' -e 's/.*INFO]://' -e 's/.*CRITICAL]://' -e 's/.*WARNING]://' \
	| grep -v 'Distributing to'
plcontainer runtime-backup -f /tmp/backup_file; cat /tmp/backup_file |  sed -e "s|${GPHOME}|GPHOME|"
echo
echo "Test runtime-replace: replace"
plcontainer runtime-replace -r runtime3 -i image2 -l r -v /host_dir3/shared3:/container_dir3/shared3:rw \
	-s use_container_network=no \
	| sed -e 's/.*ERROR]://' -e 's/.*INFO]://' -e 's/.*CRITICAL]://' -e 's/.*WARNING]://' \
	| grep -v 'Distributing to'
plcontainer runtime-backup -f /tmp/backup_file; cat /tmp/backup_file |  sed -e "s|${GPHOME}|GPHOME|"

echo
echo "Test xml validation: negative cases"

echo
echo "**Test <id> is needed"
cat >/tmp/bad_xml_file << EOF
<?xml version="1.0" ?>
<configuration>
    <runtime>
        <image>image1:0.1</image>
        <command>./client</command>
    </runtime>
</configuration>
EOF
plcontainer runtime-restore -f /tmp/bad_xml_file \
	| sed -e 's/.*ERROR]://' -e 's/.*INFO]://' -e 's/.*CRITICAL]://' -e 's/.*WARNING]://'

echo
echo "**Test: No more than 1 <id> is allowed"
cat >/tmp/bad_xml_file << EOF
<?xml version="1.0" ?>
<configuration>
    <runtime>
        <id>plc_python</id>
        <id>plc_python2</id>
        <image>image1:0.1</image>
        <command>./client</command>
    </runtime>
</configuration>
EOF
plcontainer runtime-restore -f /tmp/bad_xml_file \
	| sed -e 's/.*ERROR]://' -e 's/.*INFO]://' -e 's/.*CRITICAL]://' -e 's/.*WARNING]://'

echo
echo "**Test: <id> naming requirement"
cat >/tmp/bad_xml_file << EOF
<?xml version="1.0" ?>
<configuration>
    <runtime>
        <id>*plc_python</id>
        <image>image1:0.1</image>
        <command>./client</command>
    </runtime>
</configuration>
EOF
plcontainer runtime-restore -f /tmp/bad_xml_file \
	| sed -e 's/.*ERROR]://' -e 's/.*INFO]://' -e 's/.*CRITICAL]://' -e 's/.*WARNING]://'

echo
echo "**Test <image> is needed"
cat >/tmp/bad_xml_file << EOF
<?xml version="1.0" ?>
<configuration>
    <runtime>
        <id>plc_python</id>
        <command>./client</command>
    </runtime>
</configuration>
EOF
plcontainer runtime-restore -f /tmp/bad_xml_file \
	| sed -e 's/.*ERROR]://' -e 's/.*INFO]://' -e 's/.*CRITICAL]://' -e 's/.*WARNING]://'

echo
echo "**Test: No more than 1 <image> is allowed"
cat >/tmp/bad_xml_file << EOF
<?xml version="1.0" ?>
<configuration>
    <runtime>
        <id>plc_python</id>
        <image>image1:0.1</image>
        <image>image1:0.2</image>
        <command>./client</command>
    </runtime>
</configuration>
EOF
plcontainer runtime-restore -f /tmp/bad_xml_file \
	| sed -e 's/.*ERROR]://' -e 's/.*INFO]://' -e 's/.*CRITICAL]://' -e 's/.*WARNING]://'

echo
echo "**Test: <command> is needed"
cat >/tmp/bad_xml_file << EOF
<?xml version="1.0" ?>
<configuration>
    <runtime>
        <id>plc_python</id>
        <image>image1:0.1</image>
    </runtime>
</configuration>
EOF
plcontainer runtime-restore -f /tmp/bad_xml_file \
	| sed -e 's/.*ERROR]://' -e 's/.*INFO]://' -e 's/.*CRITICAL]://' -e 's/.*WARNING]://'

echo
echo "**Test: No more than 1 <command> is allowed"
cat >/tmp/bad_xml_file << EOF
<?xml version="1.0" ?>
<configuration>
    <runtime>
        <id>plc_python</id>
        <image>image1:0.1</image>
        <command>./client</command>
        <command>./client2</command>
    </runtime>
</configuration>
EOF
plcontainer runtime-restore -f /tmp/bad_xml_file \
	| sed -e 's/.*ERROR]://' -e 's/.*INFO]://' -e 's/.*CRITICAL]://' -e 's/.*WARNING]://'

echo
echo "**Test: <shared_directory>: 'container' attr is needed"
cat >/tmp/bad_xml_file << EOF
<?xml version="1.0" ?>
<configuration>
    <runtime>
        <id>plc_python</id>
        <image>image1:0.1</image>
        <command>./client</command>
        <shared_directory access="rw" host="/host/plcontainer_clients"/>
    </runtime>
</configuration>
EOF
plcontainer runtime-restore -f /tmp/bad_xml_file \
	| sed -e 's/.*ERROR]://' -e 's/.*INFO]://' -e 's/.*CRITICAL]://' -e 's/.*WARNING]://'

echo
echo "**Test: <shared_directory>: 'host' attr is needed"
cat >/tmp/bad_xml_file << EOF
<?xml version="1.0" ?>
<configuration>
    <runtime>
        <id>plc_python</id>
        <image>image1:0.1</image>
        <command>./client</command>
        <shared_directory access="rw" container="/clientdir"/>
    </runtime>
</configuration>
EOF
plcontainer runtime-restore -f /tmp/bad_xml_file \
	| sed -e 's/.*ERROR]://' -e 's/.*INFO]://' -e 's/.*CRITICAL]://' -e 's/.*WARNING]://'

echo
echo "**Test: <shared_directory>: 'access' attr is is needed"
cat >/tmp/bad_xml_file << EOF
<?xml version="1.0" ?>
<configuration>
    <runtime>
        <id>plc_python</id>
        <image>image1:0.1</image>
        <command>./client</command>
        <shared_directory container="/clientdir" host="/host/plcontainer_clients"/>
    </runtime>
</configuration>
EOF
plcontainer runtime-restore -f /tmp/bad_xml_file \
	| sed -e 's/.*ERROR]://' -e 's/.*INFO]://' -e 's/.*CRITICAL]://' -e 's/.*WARNING]://'

echo
echo "**Test: <shared_directory>: access must be ro or rw"
cat >/tmp/bad_xml_file << EOF
<?xml version="1.0" ?>
<configuration>
    <runtime>
        <id>plc_python</id>
        <image>image1:0.1</image>
        <command>./client</command>
        <shared_directory access="rx" container="/clientdir" host="/host/plcontainer_clients"/>
    </runtime>
</configuration>
EOF
plcontainer runtime-restore -f /tmp/bad_xml_file \
	| sed -e 's/.*ERROR]://' -e 's/.*INFO]://' -e 's/.*CRITICAL]://' -e 's/.*WARNING]://'

echo
echo "**Test: <shared_directory>: container paths should not be duplicated"
cat >/tmp/bad_xml_file << EOF
<?xml version="1.0" ?>
<configuration>
    <runtime>
        <id>plc_python</id>
        <image>image1:0.1</image>
        <command>./client</command>
        <shared_directory access="rw" container="/clientdir" host="/host/plcontainer_clients"/>
        <shared_directory access="ro" container="/clientdir" host="/host/plcontainer_clients_2"/>
    </runtime>
</configuration>
EOF
plcontainer runtime-restore -f /tmp/bad_xml_file \
	| sed -e 's/.*ERROR]://' -e 's/.*INFO]://' -e 's/.*CRITICAL]://' -e 's/.*WARNING]://'

echo
echo "**Test: <shared_directory>: container path should not be /tmp/plcontainer"
cat >/tmp/bad_xml_file << EOF
<?xml version="1.0" ?>
<configuration>
    <runtime>
        <id>plc_python</id>
        <image>image1:0.1</image>
        <command>./client</command>
        <shared_directory access="ro" container="/tmp/plcontainer" host="/host/plcontainer_clients_2"/>
    </runtime>
</configuration>
EOF
plcontainer runtime-restore -f /tmp/bad_xml_file \
	| sed -e 's/.*ERROR]://' -e 's/.*INFO]://' -e 's/.*CRITICAL]://' -e 's/.*WARNING]://'

echo
echo "**Test: <setting>: must be legal one"
cat >/tmp/bad_xml_file << EOF
<?xml version="1.0" ?>
<configuration>
    <runtime>
        <id>plc_python</id>
        <image>image1:0.1</image>
        <command>./client</command>
        <setting logging="Enable"/>
    </runtime>
</configuration>
EOF
plcontainer runtime-restore -f /tmp/bad_xml_file \
	| sed -e 's/.*ERROR]://' -e 's/.*INFO]://' -e 's/.*CRITICAL]://' -e 's/.*WARNING]://' \
	| grep -v 'Distributing to'

echo
echo "**Test: <setting>: memory_mb should be string with integer value"
cat >/tmp/bad_xml_file << EOF
<?xml version="1.0" ?>
<configuration>
    <runtime>
        <id>plc_python</id>
        <image>image1:0.1</image>
        <command>./client</command>
        <setting memory_mb="123.4"/>
    </runtime>
</configuration>
EOF
plcontainer runtime-restore -f /tmp/bad_xml_file \
	| sed -e 's/.*ERROR]://' -e 's/.*INFO]://' -e 's/.*CRITICAL]://' -e 's/.*WARNING]://'

echo
echo "**Test: <setting>: memory_mb should be string with positive integer value"
cat >/tmp/bad_xml_file << EOF
<?xml version="1.0" ?>
<configuration>
    <runtime>
        <id>plc_python</id>
        <image>image1:0.1</image>
        <command>./client</command>
        <setting memory_mb="-123"/>
    </runtime>
</configuration>
EOF
plcontainer runtime-restore -f /tmp/bad_xml_file \
	| sed -e 's/.*ERROR]://' -e 's/.*INFO]://' -e 's/.*CRITICAL]://' -e 's/.*WARNING]://'

echo
echo "**Test: <setting>: use_container_network should be yes or no"
cat >/tmp/bad_xml_file << EOF
<?xml version="1.0" ?>
<configuration>
    <runtime>
        <id>plc_python</id>
        <image>image1:0.1</image>
        <command>./client</command>
        <setting use_container_network="y"/>
    </runtime>
</configuration>
EOF
plcontainer runtime-restore -f /tmp/bad_xml_file \
	| sed -e 's/.*ERROR]://' -e 's/.*INFO]://' -e 's/.*CRITICAL]://' -e 's/.*WARNING]://'

echo
echo "**Test: <setting>: use_container_logging should be yes or no"
cat >/tmp/bad_xml_file << EOF
<?xml version="1.0" ?>
<configuration>
    <runtime>
        <id>plc_python</id>
        <image>image1:0.1</image>
        <command>./client</command>
        <setting use_container_logging="enable"/>
    </runtime>
</configuration>
EOF
plcontainer runtime-restore -f /tmp/bad_xml_file \
	| sed -e 's/.*ERROR]://' -e 's/.*INFO]://' -e 's/.*CRITICAL]://' -e 's/.*WARNING]://'
rm -f /tmp/bad_xml_file

echo
echo "Test xml validation: postive case"
cat >good_xml_file << EOF
<?xml version="1.0" ?>
<configuration>
    <runtime>
        <id>plc_python</id>
        <image>image1:0.1</image>
        <setting memory_mb="512"/>
        <setting use_container_network="NO"/>
        <setting use_container_logging="Yes"/>
        <command>./client</command>
        <shared_directory access="rw" container="/clientdir" host="/host/plcontainer_clients"/>
    </runtime>
</configuration>
EOF
plcontainer runtime-restore -f good_xml_file \
	| sed -e 's/.*ERROR]://' -e 's/.*INFO]://' -e 's/.*CRITICAL]://' -e 's/.*WARNING]://' \
	| grep -v 'Distributing to'
rm -f good_xml_file

#recover the original runtime configurations.
echo
echo "Recover the previous runtime configuration file"
plcontainer runtime-restore -f /tmp/test_backup_cfg_file_test_utility >/dev/null
rm -f /tmp/tmp.image-list.out /tmp/backup_file
# We do not remove /tmp/test_backup_cfg_file_test_utility since we might want to check it in case there are test errors.
