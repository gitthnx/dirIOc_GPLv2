
# dirIO

    Linux cmdline tool (with C sources) for directory monitoring, e.g. data io (MB/s)

![dirIO graphical output]('https://github.com/gitthnx/dirIOc_GPLv2/blob/main/tmp_C/dirIOc_v0.1.6_2025-03-26 09-36-55.png')
<!-- p align="left"> https://github.com/gitthnx/dirIO_GPLv2/blob/main/tmp/Screenshot_dirIO_light_graphical.png -->   
<br>


### syntax and keys 

    ./dirIOc 
<br> 

       Usage: ./dirIO.sh  /directory/to/monitor
                                             
              keys: on 'statx' errors == 'n'        
                    pause             == 'p'        
                    resume            == ' ' or 'r' 
                    quit              == 'q' or 'Q' 
                                             
              version 0.1                           
              June 15, 2024                         
<br>


### start
      chmod +x ./dirIO.sh
    
    ./dirIO.sh /path/to/directory/for/monitoring/data_io
<br>


### notes & issues
*1) 'graphical' visualization only partially implemented within scripts for testing functionality options in \<tmp\> directory*
    
<!-- pre><p align="left"><a href="https://github.com/gitthnx/dirIO_GPLv2"><img width="500" src="https://github.com/gitthnx/dirIO_GPLv2/blob/main/tmp/Screenshot_dirIO_light_graphical.png" /></a></p></pre -->

<pre><!-- --><img src="https://github.com/gitthnx/dirIO_GPLv2/blob/main/tmp/Screenshot_dirIO_light_graphical.png" width="500" style="margin:30px" style="padding:30px;" ></pre>

*2) experimental implementation into C source code in \<tmp_C\> directory  
    partly done by LLM_chat automated source code conversion from shell to C source code*

<!-- div id="div1" name="div1" style="position:relative; top:10; left:50;" position="absolute" top="0" left="50" ><img width="500" src="https://github.com/gitthnx/dirIO_GPLv2/blob/main/tmp/Screenshot_dirIO_light_graphical.png"></div -->

<!-- *prev2) <noscript>from \<noscript\> tag: gitREADME.md does not support JavaScript</noscript>* -->

<!-- prev3) update local repository with changes:
        git config core.fileMode true
        git pull origin main
        alternative procedure:
        git stash push --include-untracked
        git stash drop
        or:
        git reset --hard
        git pull
-->
<br>

  
### chatGPT assisted code creation, initial prompt command:
    Create a code example for data input output monitoring and data rate output within a bash shell command line. 
    Create this script as bash shell script. 
    Create this script for filesystem data input and data output and data rates from or to this directory, that is declared with script variables on startup. 
    Add request for keyboard input for stopping that script on pressing q or Q. 
    Add keyboard input scan for pausing output with pressing p and resuming with space key.
<br><br>

### credits
  inotify   
  [inotify-tools](https://github.com/inotify-tools/inotify-tools)  
  [inotify-info](https://github.com/mikesart/inotify-info)

