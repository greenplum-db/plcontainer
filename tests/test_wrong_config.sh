

f1 () {
  echo "Test lack of image"
  cat >bad_xml_file << EOF
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
  cat >bad_xml_file << EOF
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
  cat >bad_xml_file << EOF
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
  cat >bad_xml_file << EOF
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
  cat >bad_xml_file << EOF
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
  cat >bad_xml_file << EOF
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
  cat >bad_xml_file << EOF
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


function _main() {
  local config_id="${1}"
  if [ "$config_id" = "1" ]; then
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
  fi

  if [ -z "$MASTER_DATA_DIRECTORY" ]; then
	  echo "Error: MASTER_DATA_DIRECTORY is not set"
	  exit 1
  else
  	  cp bad_xml_file $MASTER_DATA_DIRECTORY/plcontainer_configuration.xml
  fi
}
_main "$@"
