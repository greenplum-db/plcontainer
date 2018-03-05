#!/bin/bash
#------------------------------------------------------------------------------
#
#
# Copyright (c) 2017-Present Pivotal Software, Inc
#
#------------------------------------------------------------------------------
#
# Test bad configuration in plcontainer C code. Called in sql/test_wrong_config.sql

f0 () {
  echo "Test lack of id"
  cat >/tmp/bad_xml_file << EOF
<?xml version="1.0" ?>
<configuration>
    <runtime>
        <image>pivotaldata/plcontainer_python:0.0</image>
        <command>./client</command>
    </runtime>
</configuration>
EOF
}

f1 () {
  echo "Test lack of image"
  cat >/tmp/bad_xml_file << EOF
<?xml version="1.0" ?>
<configuration>
    <runtime>
        <id>plc_python_shared</id>
        <command>./client</command>
    </runtime>
</configuration>
EOF
}

f2 () {
  echo "Test multiple image"
  cat >/tmp/bad_xml_file << EOF
<?xml version="1.0" ?>
<configuration>
    <runtime>
        <id>plc_python_shared</id>
        <image>pivotaldata/plcontainer_python:0.0</image>
        <image>pivotaldata/plcontainer_python:0.1</image>
        <command>./client</command>
        <shared_directory access="ro" container="/clientdir" host="/home/gpadmin/gpdb.devel/bin/plcontainer_clients"/> 
        <setting use_container_logging="yes"/>
    </runtime>
</configuration>
EOF
}

f3 () {
  echo "Test multiple id"
  cat >/tmp/bad_xml_file << EOF
<?xml version="1.0" ?>
<configuration>
    <runtime>
        <id>plc_python_shared</id>
        <id>plc_python_shared2</id>
        <image>pivotaldata/plcontainer_python:0.1</image>
        <command>./client</command>
        <shared_directory access="ro" container="/clientdir" host="/home/gpadmin/gpdb.devel/bin/plcontainer_clients"/>
        <setting use_container_logging="yes"/>
    </runtime>
</configuration>
EOF
}

f4 () {
  echo "Test lack of command element"
  cat >/tmp/bad_xml_file << EOF
<?xml version="1.0" ?>
<configuration>
    <runtime>
        <id>plc_python_shared</id>
        <image>pivotaldata/plcontainer_python:0.0</image>
        <shared_directory access="ro" container="/clientdir" host="/home/gpadmin/gpdb.devel/bin/plcontainer_clients"/>
        <setting use_container_logging="yes"/>
    </runtime>
</configuration>
EOF
}

f5 () {
  echo "Test use_container_logging values should only be yes/no"
  cat >/tmp/bad_xml_file << EOF
<?xml version="1.0" ?>
<configuration>
    <runtime>
        <id>plc_python_shared</id>
        <image>pivotaldata/plcontainer_python:0.1</image>
        <command>./client</command>
        <shared_directory access="ro" container="/clientdir" host="/home/gpadmin/gpdb.devel/bin/plcontainer_clients"/>
        <setting use_container_logging="enable"/>
    </runtime>
</configuration>
EOF
}

f6 () {
  echo "Test duplicate container path."
  cat >/tmp/bad_xml_file << EOF
<?xml version="1.0" ?>
<configuration>
    <runtime>
        <id>plc_python_shared</id>
        <image>pivotaldata/plcontainer_python:0.1</image>
        <command>./client</command>
        <shared_directory access="ro" container="/clientdir" host="/home/gpadmin/gpdb.devel/bin/plcontainer_clients"/>
        <shared_directory access="ro" container="/clientdir" host="/home/gpadmin/gpdb.devel/bin/plcontainer_clients2"/>
        <setting use_container_logging="yes"/>
    </runtime>
</configuration>
EOF
}

f7 () {
  echo "Test deleted parameter use_container_network"
  cat >/tmp/bad_xml_file << EOF
<?xml version="1.0" ?>
<configuration>
    <runtime>
        <id>plc_python_shared</id>
        <image>pivotaldata/plcontainer_python:0.1</image>
        <command>./client</command>
        <shared_directory access="ro" container="/clientdir" host="/home/gpadmin/gpdb.devel/bin/plcontainer_clients"/>
        <setting use_container_network="yes"/>
    </runtime>
</configuration>
EOF
}

f8 () {
  echo "Test duplicate id"
  cat >/tmp/bad_xml_file << EOF
<?xml version="1.0" ?>
<configuration>
    <runtime>
        <id>plc_python_shared</id>
        <id>plc_python_shared</id>
        <image>pivotaldata/plcontainer_python:0.1</image>
        <command>./client</command>
    </runtime>
</configuration>
EOF
}

f9 () {
  echo "Test use_container_logging disable"
  cat >/tmp/bad_xml_file << EOF
<?xml version="1.0" ?>
<configuration>
    <runtime>
        <id>plc_python_shared</id>
        <image>pivotaldata/plcontainer_python:0.1</image>
        <command>./client</command>
        <setting use_container_logging="disable"/>
        <setting memory_mb="512"/>
    </runtime>
</configuration>
EOF
}

f10 () {
  echo "Test wrong setting"
  cat >/tmp/bad_xml_file << EOF
<?xml version="1.0" ?>
<configuration>
    <runtime>
        <id>plc_python_shared</id>
        <image>pivotaldata/plcontainer_python:0.1</image>
        <command>./client</command>
        <setting memory="512"/>
    </runtime>
</configuration>
EOF
}

f11 () {
  echo "Test wrong memory_mb"
  cat >/tmp/bad_xml_file << EOF
<?xml version="1.0" ?>
<configuration>
    <runtime>
        <id>plc_python_shared</id>
        <image>pivotaldata/plcontainer_python:0.1</image>
        <command>./client</command>
        <setting memory_mb="-100"/>
    </runtime>
</configuration>
EOF
}

f12 () {
  echo "Test wrong element"
  cat >/tmp/bad_xml_file << EOF
<?xml version="1.0" ?>
<configuration>
    <runtime>
        <id>plc_python_shared</id>
        <images>pivotaldata/plcontainer_python:0.1</images>
        <command>./client</command>
    </runtime>
</configuration>
EOF
}

f13 () {
  echo "Test more than 1 command"
  cat >/tmp/bad_xml_file << EOF
<?xml version="1.0" ?>
<configuration>
    <runtime>
        <id>plc_python_shared</id>
        <image>pivotaldata/plcontainer_python:0.1</image>
        <command>./client</command>
        <command>./client2</command>
    </runtime>
</configuration>
EOF
}

f14 () {
  echo "Test 'host' missing in shared_directory"
  cat >/tmp/bad_xml_file << EOF
<?xml version="1.0" ?>
<configuration>
    <runtime>
        <id>plc_python_shared</id>
        <image>pivotaldata/plcontainer_python:0.1</image>
        <command>./client</command>
        <shared_directory access="ro" container="/clientdir"/> 
    </runtime>
</configuration>
EOF
}

f15 () {
  echo "Test 'container' missing in shared_directory"
  cat >/tmp/bad_xml_file << EOF
<?xml version="1.0" ?>
<configuration>
    <runtime>
        <id>plc_python_shared</id>
        <image>pivotaldata/plcontainer_python:0.1</image>
        <command>./client</command>
        <shared_directory access="ro" host="/home/gpadmin/gpdb.devel/bin/plcontainer_clients"/> 
    </runtime>
</configuration>
EOF
}

f16 () {
  echo "Test 'access' missing in shared_directory"
  cat >/tmp/bad_xml_file << EOF
<?xml version="1.0" ?>
<configuration>
    <runtime>
        <id>plc_python_shared</id>
        <image>pivotaldata/plcontainer_python:0.1</image>
        <command>./client</command>
        <shared_directory container="/clientdir" host="/home/gpadmin/gpdb.devel/bin/plcontainer_clients"/> 
    </runtime>
</configuration>
EOF
}

f17 () {
  echo "Test bad access in shared_directory"
  cat >/tmp/bad_xml_file << EOF
<?xml version="1.0" ?>
<configuration>
    <runtime>
        <id>plc_python_shared</id>
        <image>pivotaldata/plcontainer_python:0.1</image>
        <command>./client</command>
        <shared_directory access="rx" container="/clientdir" host="/home/gpadmin/gpdb.devel/bin/plcontainer_clients"/> 
    </runtime>
</configuration>
EOF
}

f18 () {
  echo "Test long runtime id which exceeds the length limit"
  cat >/tmp/bad_xml_file << EOF
<?xml version="1.0" ?>
<configuration>
    <runtime>
        <id>plc_python_shared_toooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooo_long</id>
        <image>pivotaldata/plcontainer_python:0.1</image>
        <command>./client</command>
        <shared_directory access="ro" container="/clientdir" host="/home/gpadmin/gpdb.devel/bin/plcontainer_clients"/>
        <setting use_container_logging="yes"/>
    </runtime>
</configuration>
EOF
}

f19 () {
  echo "Test wrong cpu_share name"
  cat >/tmp/bad_xml_file << EOF
<?xml version="1.0" ?>
<configuration>
    <runtime>
        <id>plc_python_shared</id>
        <image>pivotaldata/plcontainer_python:0.1</image>
        <command>./client</command>
        <setting cpu="100"/>
    </runtime>
</configuration>
EOF
}

f20 () {
  echo "Test wrong cpu_share value"
  cat >/tmp/bad_xml_file << EOF
<?xml version="1.0" ?>
<configuration>
    <runtime>
        <id>plc_python_shared</id>
        <image>pivotaldata/plcontainer_python:0.1</image>
        <command>./client</command>
        <setting cpu_share="-100"/>
    </runtime>
</configuration>
EOF
}

f21 () {
	echo "Test good format (but it still fails since the configuration (image/command) is not legal)"
  cat >/tmp/bad_xml_file << EOF
<?xml version="1.0" ?>
<configuration>
    <runtime>
        <id>plc_python_shared</id>
        <image>not_exist_pivotaldata/plcontainer_python:0.1</image>
        <command>./not_exist_client</command>
        <shared_directory access="rw" container="/clientdir1" host="/home/gpadmin/gpdb.devel/bin/plcontainer_clients1"/> 
        <shared_directory access="ro" container="/clientdir2" host="/home/gpadmin/gpdb.devel/bin/plcontainer_clients2"/> 
        <setting use_container_logging="no"/>
        <setting memory_mb="512"/>
        <setting cpu_share="1024"/>
    </runtime>
</configuration>
EOF
}

function _main() {
  local config_id="${1}"
  if [ "$config_id" = "0" ]; then
	  f0
  elif [ "$config_id" = "1" ]; then
	  f1
  elif [ "$config_id" = "2" ]; then
	  f2
  elif [ "$config_id" = "3" ]; then
	  f3
  elif [ "$config_id" = "4" ]; then
	  f4
  elif [ "$config_id" = "5" ]; then
	  f5
  elif [ "$config_id" = "6" ]; then
	  f6
  elif [ "$config_id" = "7" ]; then
	  f7
  elif [ "$config_id" = "8" ]; then
	  f8
  elif [ "$config_id" = "9" ]; then
	  f9
  elif [ "$config_id" = "10" ]; then
	  f10
  elif [ "$config_id" = "11" ]; then
	  f11
  elif [ "$config_id" = "12" ]; then
	  f12
  elif [ "$config_id" = "13" ]; then
	  f13
  elif [ "$config_id" = "14" ]; then
	  f14
  elif [ "$config_id" = "15" ]; then
	  f15
  elif [ "$config_id" = "16" ]; then
	  f16
  elif [ "$config_id" = "17" ]; then
	  f17
  elif [ "$config_id" = "18" ]; then
	  f18
  elif [ "$config_id" = "19" ]; then
	  f19
  elif [ "$config_id" = "20" ]; then
	  f20
  elif [ "$config_id" = "21" ]; then
	  f21
  fi

  if [ -z "$MASTER_DATA_DIRECTORY" ]; then
	  echo "Error: MASTER_DATA_DIRECTORY is not set"
	  exit 1
  else
  	  cp /tmp/bad_xml_file $MASTER_DATA_DIRECTORY/plcontainer_configuration.xml
	  rm -f /tmp/bad_xml_file
  fi
}
_main "$@"
