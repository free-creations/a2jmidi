# Build and install
This installation procedure explains how to compile and install the
software on a Linux system. 
The procedure has been tested on Ubuntu Focal Fossa (20.04 LTS).
For other Linux distributions, some adjustments might be required.

## Step 1: Make sure you have all the necessary development tools
    
Open a console and enter:

	$ sudo apt-get install build-essential
	$ sudo apt-get install cmake
    $ sudo apt-get install libjack-jackd2-dev
    $ sudo apt-get install libasound2-dev
    $ sudo apt-get install libboost-program-options-dev
    
## Step 2: get the sources

Open a console and enter:

    $ git clone --recurse-submodules https://github.com/free-creations/a_j_midi.git
    
Note: if you pass `--recurse-submodules` to the `git clone` command, 
it will automatically initialize and update the 
[gtest](https://github.com/google/googletest) test-framework
and the 
[Spdlog](https://github.com/gabime/spdlog) logging-framework.
    
## Step 3: Configure the project 

	$ mkdir build
	$ cd build
	$ cmake -DCMAKE_INSTALL_PREFIX=/usr ../

## Step 4: compile the source

	$ make
	
## Step 5: install
           
     $ sudo make install
