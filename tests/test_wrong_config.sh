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
        <setting logs="enable"/>
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
        <setting logs="enable"/>
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
        <setting logs="enable"/>
    </runtime>
</configuration>
EOF
}

f5 () {
  echo "Test log values should only be enable/disable"
  cat >/tmp/bad_xml_file << EOF
<?xml version="1.0" ?>
<configuration>
    <runtime>
        <id>plc_python_shared</id>
        <image>pivotaldata/plcontainer_python:0.1</image>
        <command>./client</command>
        <shared_directory access="ro" container="/clientdir" host="/home/gpadmin/gpdb.devel/bin/plcontainer_clients"/>
        <setting logs="yes"/>
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
        <setting logs="enable"/>
    </runtime>
</configuration>
EOF
}

f7 () {
  echo "Test wrong use_network value."
  cat >/tmp/bad_xml_file << EOF
<?xml version="1.0" ?>
<configuration>
    <runtime>
        <id>plc_python_shared</id>
        <image>pivotaldata/plcontainer_python:0.1</image>
        <command>./client</command>
        <shared_directory access="ro" container="/clientdir" host="/home/gpadmin/gpdb.devel/bin/plcontainer_clients"/>
        <setting logs="enable"/>
        <setting use_network="enable"/>
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
  echo "Test logs/use_network disable"
  cat >/tmp/bad_xml_file << EOF
<?xml version="1.0" ?>
<configuration>
    <runtime>
        <id>plc_python_shared</id>
        <image>pivotaldata/plcontainer_python:0.1</image>
        <command>./client</command>
        <setting logs="disable"/>
        <setting use_network="disable"/>
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
