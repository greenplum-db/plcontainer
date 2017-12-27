

f1 () {
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
  fi
  cp bad_xml_file $MASTER_DATA_DIRECTORY/plcontainer_configuration.xml
}
_main "$@"
