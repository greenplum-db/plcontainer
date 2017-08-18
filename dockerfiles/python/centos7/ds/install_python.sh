pushd /opt/ds
rpm2cpio DataSciencePython-*.x86_64.rpm| cpio -div
mv temp/ext/DataSciencePython/* ./
popd

source /opt/python_env.sh
python --version
