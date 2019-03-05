Summary:        JSON-C library 
License:        MIT License        
Name:           json-c
Version:        %{json_ver}
Release:        %{json_rel}
Group:          Development/Tools
Prefix:         /temp
AutoReq:        no
AutoProv:       no
Provides:       json-c = %{json_ver} 

%description
The JSON-C module provides a JSON implementation in C.

%install
mkdir -p %{buildroot}/temp/lib
cp -d /usr/local/lib/libjson-c.so* %{buildroot}/temp/lib/

%files
/temp
