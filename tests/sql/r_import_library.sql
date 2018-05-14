CREATE OR REPLACE FUNCTION rint2(i int2) RETURNS int2 AS $$
# container: plc_r_shared
library()
library(BH)
library(DBI)
library(MASS)
library(MCMCpack)
library(Matrix)
library(rjags)
library(R2jags)
library(R6)
library(RColorBrewer)
library(ROCR)
library(Rcpp)
library(RcppEigen)
library(RobustRankAggreg)
library(SparseM)
library(abind)
library(adabag)
library(arm)
library(assertthat)
library(bitops)
library(caTools)
library(car)
library(caret)
library(coda)
library(colorspace)
library(compHclust)
library(curl)
library(data.table)
library(dichromat)
library(digest)
library(dplyr)
library(e1071)
library(flashClust)
library(foreign)
library(gdata)
library(ggplot2)
library(glmnet)
library(gplots)
library(gtable)
library(gtools)
library(hms)
library(hybridHclust)
library(igraph)
library(labeling)
library(lattice)
library(lazyeval)
library(lme4)
library(lmtest)
library(magrittr)
library(minqa)
library(munsell)
library(neuralnet)
library(nloptr)
library(nnet)
library(pbkrtest)
library(plyr)
library(quantreg)
library(randomForest)
library(readr)
library(reshape2)
library(rpart)
library(sandwich)
library(scales)
library(stringi)
library(stringr)
library(survival)
library(tibble)
library(tseries)
library(zoo)
library(MTS)
library(forecast)
library(RPostgreSQL)
set.seed(101)
if (names(dev.cur()) != "null device") dev.off()
pdf("/tmp/Rplots.pdf")
x <- matrix(rnorm(500),5,100)
x <- rbind(x,x[rep(1,4),]+matrix(rnorm(400),4,100))
x <- rbind(x,x[2:5,]+matrix(rnorm(400),4,100))
par(mfrow=c(1,2))
e1 <- eisenCluster(x,'correlation')

#nloptr
eval_f <- function(y) {
    return( 100 * (y[2] - y[1] * y[1])^2 + (1 - y[1])^2 )
}

eval_grad_f <- function(y) {
    return( c( -400 * y[1] * (y[2] - y[1] * y[1]) - 2 * (1 - y[1]),
                200 * (y[2] - y[1] * y[1])) )
}

x0 <- c( -1.2, 1 )

opts <- list("algorithm"="NLOPT_LD_LBFGS",
             "xtol_rel"=1.0e-8)

res <- nloptr( x0=x0,
               eval_f=eval_f,
               eval_grad_f=eval_grad_f,
               opts=opts)
eval_f_list <- function(y) {
    return( list( "objective" = 100 * (y[2] - y[1] * y[1])^2 + (1 - y[1])^2,
                  "gradient"  = c( -400 * y[1] * (y[2] - y[1] * y[1]) - 2 * (1 - y[1]),
                                    200 * (y[2] - y[1] * y[1])) ) )
}

# solve Rosenbrock Banana function using an objective function that
# returns a list with the objective value and its gradient
res <- nloptr( x0=x0,
               eval_f=eval_f_list,
               opts=opts)

#stringr
str_length("abc")
sx <- c("abcdef", "ghifjk")
str_sub(sx, 3, 3)

#hms
hms(666)

#digest
digest("r algorithm", algo="sha256")

#pbkrtest
data(beets, package="pbkrtest")
head(beets)

## Linear mixed effects model:
sug   <- lmer(sugpct ~ block + sow + harvest + (1|block:harvest), data=beets, REML=FALSE)
sug.h <- update(sug, .~. -harvest)
sug.s <- update(sug, .~. -sow)

#RcppEigen
Rsamp <- function(X) {
        stopifnot(is.numeric(X <- as.matrix(X)),
                     (nc <- ncol(X)) > 1L,
                          all(X >= 0))
        apply(X, 1L, function(x) sample(nc, size=1L, replace=FALSE, prob=x+1e-10))
}

#abind
xx <- matrix(1:12,3,4)
yy <- xx+100
dim(abind(xx,yy,along=0))     # binds on new dimension before first
dim(abind(xx,yy,along=1))     # binds on first dimension
dim(abind(xx,yy,along=1.5))

return (as.integer(i))
$$ LANGUAGE plcontainer;

select rint2(1::int2);

DROP FUNCTION rint2(i int2);
