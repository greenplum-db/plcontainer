pushd /opt/ds
rpm2cpio DataSciencePython-*.x86_64.rpm| cpio -di
mv temp/ext/DataSciencePython/* ./
popd

source /opt/python_env.sh
python --version
