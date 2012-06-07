Description
===========

The Black-Scholes equation is a differential equation that describes how,
under a certain set of assumptions, the value of an option changes as the 
price of the underlying asset changes.

The formula for a put option is similar. The cumulative normal distribution 
function, CND(x), gives the probability that normally distributed random
variable will have a value less than x. There is no closed form expression for
this function, and as such it must be evaluated numerically. The other 
parameters are as follows: S underlying asset's current price, 
X the strike price, T time to the expiration date, r risk-less rate of return,
and v stock's volatility.

Blackscholes has a single step collection called (compute). It is executed once
for each tag in the tag collection <tags>. This tag collection is produced
by the environment.

(compute) inputs [data] items. Each [data] item contains a pointer to an 
array of ParameterSet objects. For each ParameterSet object the Black-Scholes equation 
is solved, and results are stored in the emitted items [price].

To reduce the influence of the serial part each calculation is repeated for NUM_RUNS times.

There are no constraints on the parallelism in this example. An instance of
(compute) does not rely on a data item from another instance and it does not
relay on a control tag from another instance. All the tags are produced by
the environment before the program begins execution. All steps can be executed
in parallel if there are enough processors.


DistCnC enabling
================

blackscholes.cpp/h is distCnC-ready.
Use the preprocessor definition _DIST_ to enable distCnC.
    
See the runtime_api reference for how to run distCnC programs.


Usage
=====

The command line is:

blackscholes n b t
    n  : positive integer for the number of options
    b  : positive integer for the size of blocks
    t  : positive integer for the number of threads
    
e.g.
blackscholes 100000 100 4
