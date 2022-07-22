#!/bin/bash
apt-get update
DEBIAN_FRONTEND=noninteractive apt-get -y install gnupg2 apt-utils python3-pip
DEBIAN_FRONTEND=noninteractive apt-get -y install --reinstall ca-certificates && rm -rf /var/lib/apt/lists/*
# ubuntu20.04 cmake version < 3.18 use pip install to make it easy 
pip install cmake

apt-key adv --keyserver keyserver.ubuntu.com --recv-keys E298A3A825C0D65DFD57CBB651716619E084DAB9
echo 'deb https://cloud.r-project.org/bin/linux/ubuntu bionic-cran35/' >> /etc/apt/sources.list

apt-get update
DEBIAN_FRONTEND=noninteractive apt-get -y install r-base r-base-dev wget pkg-config jags libcurl4-openssl-dev libpq-dev libxml2-dev

source /opt/r_env.sh
R --version

pushd /opt
chmod +x /opt/install_r_package.R

wget https://cran.r-project.org/src/contrib/Archive/nloptr/nloptr_1.0.4.tar.gz
R CMD INSTALL nloptr_1.0.4.tar.gz
wget https://github.com/gagolews/stringi/archive/v1.1.6.tar.gz -O stringi.tar.gz
R CMD INSTALL stringi.tar.gz
wget https://github.com/tidyverse/glue/archive/v1.2.0.tar.gz -O glue.tar.gz
R CMD INSTALL glue.tar.gz
wget https://github.com/tidyverse/magrittr/archive/v.1.5.tar.gz -O magrittr.tar.gz
R CMD INSTALL magrittr.tar.gz
wget https://github.com/tidyverse/stringr/archive/v1.3.0.tar.gz -O stringr.tar.gz
R CMD INSTALL stringr.tar.gz
wget https://cran.r-project.org/src/contrib/Archive/mvtnorm/mvtnorm_1.0-8.tar.gz
R CMD INSTALL mvtnorm_1.0-8.tar.gz
wget https://cran.r-project.org/src/contrib/Archive/data.table/data.table_1.11.8.tar.gz
R CMD INSTALL data.table_1.11.8.tar.gz
wget https://cran.r-project.org/src/contrib/Archive/pkgconfig/pkgconfig_2.0.2.tar.gz
R CMD INSTALL pkgconfig_2.0.2.tar.gz
wget https://cran.r-project.org/src/contrib/Archive/igraph/igraph_1.2.2.tar.gz
R CMD INSTALL igraph_1.2.2.tar.gz
wget https://cran.r-project.org/src/contrib/Archive/quadprog/quadprog_1.5-5.tar.gz
R CMD INSTALL quadprog_1.5-5.tar.gz

# new package for PL/Container only
echo 'stinepack' >> /opt/rlibs
echo 'imputeTS' >> /opt/rlibs
echo 'knitr' >> /opt/rlibs
echo 'rmarkdown' >> /opt/rlibs
echo 'gglasso' >> /opt/rlibs
echo 'car' >> /opt/rlibs

input="/opt/rlibs"
while IFS= read -r package
do
  if [ ! -d $DESTINATION/$package ]; then
     /opt/install_r_package.R $package
  else
    echo "$package installed"
  fi
done < "$input"

input="/opt/rlibs.higher_gcc"
while IFS= read -r package
do
  if [ ! -d $DESTINATION/$package ]; then
     /opt/install_r_package.R $package
  else
    echo "$package installed"
  fi
done < "$input"

# install later to avoid dependency issue
wget https://cran.r-project.org/src/contrib/Archive/lubridate/lubridate_1.7.3.tar.gz
R CMD INSTALL lubridate_1.7.3.tar.gz
wget https://cran.r-project.org/src/contrib/Archive/mcmc/mcmc_0.9-6.tar.gz
R CMD INSTALL mcmc_0.9-6.tar.gz
wget https://cran.r-project.org/src/contrib/Archive/MCMCpack/MCMCpack_1.4-5.tar.gz
R CMD INSTALL MCMCpack_1.4-5.tar.gz
wget https://cran.r-project.org/src/contrib/Archive/glmnet/glmnet_2.0-18.tar.gz
R CMD INSTALL glmnet_2.0-18.tar.gz
wget https://cran.r-project.org/src/contrib/Archive/hybridHclust/hybridHclust_1.0-5.tar.gz
R CMD INSTALL hybridHclust_1.0-5.tar.gz

# The newest package, URL may change if new version available
wget https://cran.r-project.org/src/contrib/Archive/PivotalR/PivotalR_0.1.18.4.tar.gz 
R CMD INSTALL PivotalR_0.1.18.4.tar.gz

rm *.tar.gz

popd
