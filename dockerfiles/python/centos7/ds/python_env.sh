#setup PYTHONHOME
PYTHONHOME="/opt/python/rhel6_x86_64/python-2.7.12"
PYTHONPATH=/opt/python/rhel6_x86_64/python-2.7.12/lib/python
PYTHONPATH=/opt/ds/lib/python2.7/site-packages:$PYTHONPATH
PATH=$PYTHONHOME/bin:$PATH
LD_LIBRARY_PATH=$PYTHONHOME/lib:$LD_LIBRARY_PATH
export PATH
export LD_LIBRARY_PATH
export PYTHONPATH
export PYTHONHOME
