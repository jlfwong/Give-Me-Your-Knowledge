      _____ _             __  __       __     __              
     / ____(_)           |  \/  |      \ \   / /              
    | |  __ ___   _____  | \  / | ___   \ \_/ /__  _   _ _ __ 
    | | |_ | \ \ / / _ \ | |\/| |/ _ \   \   / _ \| | | | '__|
    | |__| | |\ V /  __/ | |  | |  __/    | | (_) | |_| | |   
     \_____|_| \_/ \___| |_|  |_|\___|    |_|\___/ \__,_|_|   
                                                              
                                                              
     _  __                    _          _            _ 
    | |/ /                   | |        | |          | |
    | ' / _ __   _____      _| | ___  __| | __ _  ___| |
    |  < | '_ \ / _ \ \ /\ / / |/ _ \/ _` |/ _` |/ _ \ |
    | . \| | | | (_) \ V  V /| |  __/ (_| | (_| |  __/_|
    |_|\_\_| |_|\___/ \_/\_/ |_|\___|\__,_|\__, |\___(_)
                                            __/ |       
                                           |___/        

    ==== Nov 23: Test Automation and Test Driven Development ===

Improving your Testing Methodology
==================================
For the purposes of demonstration, the following examples deal with testing 
the MIPS assembler written for [CS241][] Assignments 3 and 4.


**Manual Input, Manual Verification, No Reporting**

    ./asm | xxd -b

**Automated Input, Manual Verification, No Reporting**

    ./asm < test.in | xxd -b -c4

**Automated Input, Automated Verification, Poor Reporting**

    ./asm < test.in > test.asm.out
    ./asm < test.in > test.binasm.out
    diff test.asm.out test.binasm.out

*OR*
  
    diff <(./asm < test.in) <(java cs241.binasm < test.in)

**Automated Input, Automated Verification, Better Reporting**

    diff -y <(./asm < test.in | xxd -b -c4) <(java cs241.binasm < test.in | xxd -b -c4)
 
[CS241]: http://www.student.cs.uwaterloo.ca/~cs241/

Next Steps: Building a Simple Test System
=========================================
* Create a way to handle multiple test cases - read all files from a directory for instance.
* Check that errors are handled correctly as well as output (check return codes)
* If a test fails, provide useful feedback
* Facilitate running individual tests or groups of tests

Trying out CS241 Testing with Rake
==================================

This requires a bit of setup

You'll need to install git, ruby and rubygems to get this working (don't worry, they're all
useful things to have on your system)

* [http://git-scm.com/download]()
* [http://www.ruby-lang.org/en/]()
* [https://rubygems.org/pages/download]()

Once you have rubygems installed, you'll need to install some gems
To do this, run

    sudo gem install rainbow
    sudo gem install rake

You'll also need to have local running copies of all the CS241 java binaries, which are
conveniently bundled as `cs241javabin.tar.gz` in this repository. 
To get them up and running in your class path, extract them somewhere, say to `/home/YOURUSERNAME/cs241/classes` then add the following
to your `.bash_profile`.

    export CLASSPATH=.:/home/YOURUSERNAME/cs241/classes:$CLASSPATH

Once you have that done, grab this repo by running

    git clone git://github.com/phleet/Give-Me-Your-Knowledge.git

Or alternatively click the Downloads button at the top of this page and extract the achive.

Inside the repository, which you can see above, there is the C starter code for Assignment 9.

I've provided a Makefile which will build this for you, so simply run `make` in this directory.

This should produce a `./wlgen` executable.

Finally, you can run the test suite by running

    rake

If all went well, you should see the following output in nice colour:

    A9P1
    ====
    FAILED: err_return_c.yaml invalid but wlgen did not error
    PASSED: All trials passed for return_a.yaml
    PASSED: All trials passed for return_b.yaml

To get a feel for what the framework is capable of, take a look inside the `Rakefile` and the `testcases` folder.

Happy Hacking!

Some Example Test Frameworks
============================
* [QUnit][]: JavaScript unit testing
* [Test::Unit][]: Ruby unit testing (Used in Ruby-on-Rails)
* [RSpec][]: Descriptive testing for Ruby
* [Selenium][]: Cross-browser web interaction testing

[QUnit]: http://docs.jquery.com/Qunit 
[Test::Unit]: http://apidock.com/ruby/Test/Unit
[RSPec]: http://rspec.info/
[Selenium]: http://seleniumhq.org/
