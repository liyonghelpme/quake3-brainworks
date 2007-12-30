Installation and compiling:

To run this code, you will obviously need a copy of Quake 3 installed.  Because
of how the original Quake 3 source code release was setup, you will need Quake 3
installed in the root directory of a windows drive (eg. C:\quake3 or c:\quake3).
As best I can tell, the only files which require this pathing structure are:

code/game/game.bat
          game.q3asm
          game_ta.bat
          game_ta.q3asm
          
It should be easy to modify these files to access another directory structure.

Checkout the repository under the quake3 directory.  For example, as:
  quake3/source.brainworks

For the latest release, copy the contents of the release directory from
quake3/source.brainworks/release to quake3/brainworks.  You will also need
to create the quake3/brainworks/vm directory so the assembler will have a
place to store the compiled code.

To compile the code, open a command shell and change to the game directory, and
then run the "game" batch file:

  cd \quake3\source.brainworks\code\game
  game
    
That batch file will fail if you haven't added the "lcc" command to your path.
Add the bin directory of the repository to your path:
  \quake3\source.brainworks\bin
Alternatively, modify the game.bat file to use the fully qualified lcc.exe
path, wherever you installed it.

Upon success, the batch script will create a new VM file in at:
  quake3/brainworks/vm/qagame.qvm
   
Then add the vm/qagame.qvm file (with that directory pathing) to the mod file:
  quake3/brainworks/brainworks.pk3
  
Remember that .pk3 files are just .zip files with a different extension.