#include "cmdpp/shell.hpp"

#include <string_view>

namespace cmdpp {

std::string_view command_help(std::string_view key) {
    static const std::string_view dir_text =
        "Displays a list of files and subdirectories in a directory.\n"
        "\n"
        "DIR [drive:][path][filename] [/P] [/W] [/A[[:]attributes]]\n"
        "  [/O[[:]sortorder]] [/S] [/B] [/L] [/V]\n"
        "\n"
        "  [drive:][path][filename]\n"
        "              Specifies drive, directory, and/or files to list.\n"
        "  /P          Pauses after each screenful of information.\n"
        "  /W          Uses wide list format.\n"
        "  /A          Displays files with specified attributes.\n"
        "  attributes   D  Directories                R  Read-only files\n"
        "               H  Hidden files               A  Files ready for archiving\n"
        "               S  System files               -  Prefix meaning not\n"
        "  /O          List by files in sorted order.\n"
        "  sortorder    N  By name (alphabetic)       S  By size (smallest first)\n"
        "               E  By extension (alphabetic)  D  By date & time (earliest first)\n"
        "               G  Group directories first    -  Prefix to reverse order\n"
        "  /S          Displays files in specified directory and all subdirectories.\n"
        "  /B          Uses bare format (no heading information or summary).\n"
        "  /L          Uses lowercase.\n"
        "  /V          Verbose mode.\n"
        "\n"
        "Switches may be preset in the DIRCMD environment variable.  Override\n"
        "preset switches by prefixing any switch with - (hyphen)--for example, /-W.\n";

    static const std::string_view call_text =
        "Calls one batch program from another.\n"
        "\n"
        "CALL [drive:][path]filename [batch-parameters]\n"
        "\n"
        "  batch-parameters   Specifies any command-line information required by the\n"
        "                     batch program.\n";

    static const std::string_view chcp_text =
        "Displays or sets the active code page number.\n"
        "\n"
        "CHCP [nnn]\n"
        "\n"
        "  nnn   Specifies a code page number.\n"
        "\n"
        "Type CHCP without a parameter to display the active code page number.\n";

    static const std::string_view rename_text =
        "Renames a file/directory or files/directories.\n"
        "\n"
        "RENAME [drive:][path][directoryname1 | filename1] [directoryname2 | filename2]\n"
        "REN [drive:][path][directoryname1 | filename1] [directoryname2 | filename2]\n"
        "\n"
        "Note that you cannot specify a new drive or path for your destination.\n";

    static const std::string_view del_text =
        "Deletes one or more files.\n"
        "\n"
        "DEL [drive:][path]filename [/P]\n"
        "ERASE [drive:][path]filename [/P]\n"
        "\n"
        "  [drive:][path]filename  Specifies the file(s) to delete.  Specify multiple\n"
        "                          files by using wildcards.\n"
        "  /P                      Prompts for confirmation before deleting each file.\n";

    static const std::string_view type_text =
        "Displays the contents of text files.\n"
        "\n"
        "TYPE [drive:][path]filename\n";

    static const std::string_view rem_text =
        "Records comments (remarks) in a batch file or CONFIG.SYS.\n"
        "\n"
        "REM [comment]\n";

    static const std::string_view copy_text =
        "Copies one or more files to another location.\n"
        "\n"
        "COPY [/A | /B] source [/A | /B] [+ source [/A | /B] [+ ...]] [destination\n"
        "  [/A | /B]] [/V] [/Y | /-Y]\n"
        "\n"
        "  source       Specifies the file or files to be copied.\n"
        "  /A           Indicates an ASCII text file.\n"
        "  /B           Indicates a binary file.\n"
        "  destination  Specifies the directory and/or filename for the new file(s).\n"
        "  /V           Verifies that new files are written correctly.\n"
        "  /Y           Suppresses prompting to confirm you want to overwrite an\n"
        "               existing destination file.\n"
        "  /-Y          Causes prompting to confirm you want to overwrite an\n"
        "               existing destination file.\n"
        "\n"
        "The switch /Y may be preset in the COPYCMD environment variable.\n"
        "This may be overridden with /-Y on the command line.\n"
        "To append files, specify a single file for destination, but multiple files\n"
        "for source (using wildcards or file1+file2+file3 format).\n";

    static const std::string_view pause_text =
        "Suspends processing of a batch program and displays the message:\n"
        "\n"
        "Press any key to continue . . .\n"
        "\n"
        "PAUSE\n";

    static const std::string_view date_text =
        "Displays or sets the date.\n"
        "\n"
        "DATE [date]\n"
        "\n"
        "Type DATE without parameters to display the current date setting and\n"
        "a prompt for a new one.  Press ENTER to keep the same date.\n";

    static const std::string_view time_text =
        "Displays or sets the system time.\n"
        "\n"
        "TIME [time]\n"
        "\n"
        "Type TIME with no parameters to display the current time setting and a prompt\n"
        "for a new one.  Press ENTER to keep the same time.\n";

    static const std::string_view ver_text =
        "Displays the Windows version.\n"
        "\n"
        "VER\n";

    static const std::string_view vol_text =
        "Displays the disk volume label and serial number, if they exist.\n"
        "\n"
        "VOL [drive:]\n";

    static const std::string_view chdir_text =
        "Displays the name of or changes the current directory.\n"
        "\n"
        "CHDIR [drive:][path]\n"
        "CHDIR[..]\n"
        "CD [drive:][path]\n"
        "CD[..]\n"
        "\n"
        "  ..   Specifies that you want to change to the parent directory.\n"
        "\n"
        "Type CD drive: to display the current directory in the specified drive.\n"
        "Type CD without parameters to display the current drive and directory.\n";

    static const std::string_view mkdir_text =
        "Creates a directory.\n"
        "\n"
        "MKDIR [drive:]path\n"
        "MD [drive:]path\n";

    static const std::string_view rmdir_text =
        "Removes (deletes) a directory.\n"
        "\n"
        "RMDIR [drive:]path\n"
        "RD [drive:]path\n";

    static const std::string_view break_text =
        "Sets or clears extended CTRL+C checking.\n"
        "\n"
        "BREAK [ON | OFF]\n"
        "\n"
        "Type BREAK without a parameter to display the current BREAK setting.\n";

    static const std::string_view verify_text =
        "Tells Windows whether to verify that your files are written correctly to a\n"
        "disk.\n"
        "\n"
        "VERIFY [ON | OFF]\n"
        "\n"
        "Type VERIFY without a parameter to display the current VERIFY setting.\n";

    static const std::string_view set_text =
        "Displays, sets, or removes Windows environment variables.\n"
        "\n"
        "SET [variable=[string]]\n"
        "\n"
        "  variable  Specifies the environment-variable name.\n"
        "  string    Specifies a series of characters to assign to the variable.\n"
        "\n"
        "Type SET without parameters to display the current environment variables.\n";

    static const std::string_view prompt_text =
        "Changes the Windows command prompt.\n"
        "\n"
        "PROMPT [text]\n"
        "\n"
        "  text    Specifies a new command prompt.\n"
        "\n"
        "Prompt can be made up of normal characters and the following special codes:\n"
        "\n"
        "  $Q   = (equal sign)\n"
        "  $$   $ (dollar sign)\n"
        "  $T   Current time\n"
        "  $D   Current date\n"
        "  $P   Current drive and path\n"
        "  $V   Windows version number\n"
        "  $N   Current drive\n"
        "  $G   > (greater-than sign)\n"
        "  $L   < (less-than sign)\n"
        "  $B   | (pipe)\n"
        "  $H   Backspace (erases previous character)\n"
        "  $E   Escape code (ASCII code 27)\n"
        "  $_   Carriage return and linefeed\n"
        "\n"
        "Type PROMPT without parameters to reset the prompt to the default setting.\n";

    static const std::string_view path_text =
        "Displays or sets a search path for executable files.\n"
        "\n"
        "PATH [[drive:]path[;...]]\n"
        "PATH ;\n"
        "\n"
        "Type PATH ; to clear all search-path settings and direct Windows to search\n"
        "only in the current directory.\n"
        "Type PATH without parameters to display the current path.\n";

    static const std::string_view exit_text =
        "Quits the COMMAND.COM program (command interpreter).\n"
        "\n"
        "EXIT\n";

    static const std::string_view ctty_text =
        "Changes the terminal device used to control your system.\n"
        "\n"
        "CTTY device\n"
        "\n"
        "  device   The terminal device you want to use, such as COM1.\n";

    static const std::string_view echo_text =
        "Displays messages, or turns command-echoing on or off.\n"
        "\n"
        "  ECHO [ON | OFF]\n"
        "  ECHO [message]\n"
        "\n"
        "Type ECHO without parameters to display the current echo setting.\n";

    static const std::string_view goto_text =
        "Directs Windows to a labelled line in a batch program.\n"
        "\n"
        "GOTO label\n"
        "\n"
        "  label   Specifies a text string used in the batch program as a label.\n"
        "\n"
        "You type a label on a line by itself, beginning with a colon.\n";

    static const std::string_view shift_text =
        "Changes the position of replaceable parameters in a batch file.\n"
        "\n"
        "SHIFT\n";

    static const std::string_view if_text =
        "Performs conditional processing in batch programs.\n"
        "\n"
        "IF [NOT] ERRORLEVEL number command\n"
        "IF [NOT] string1==string2 command\n"
        "IF [NOT] EXIST filename command\n"
        "\n"
        "  NOT               Specifies that Windows should carry out the command only\n"
        "                    if the condition is false.\n"
        "  ERRORLEVEL number Specifies a true condition if the last program run returned\n"
        "                    an exit code equal to or greater than the number specified.\n"
        "  command           Specifies the command to carry out if the condition is\n"
        "                    met.\n"
        "  string1==string2  Specifies a true condition if the specified text strings\n"
        "                    match.\n"
        "  EXIST filename    Specifies a true condition if the specified filename\n"
        "                    exists.\n";

    static const std::string_view for_text =
        "Runs a specified command for each file in a set of files.\n"
        "\n"
        "FOR %variable IN (set) DO command [command-parameters]\n"
        "\n"
        "  %variable  Specifies a replaceable parameter.\n"
        "  (set)      Specifies a set of one or more files.  Wildcards may be used.\n"
        "  command    Specifies the command to carry out for each file.\n"
        "  command-parameters\n"
        "             Specifies parameters or switches for the specified command.\n"
        "\n"
        "To use the FOR command in a batch program, specify %%variable instead of\n"
        "%variable.\n";

    static const std::string_view cls_text =
        "Clears the screen.\n"
        "\n"
        "CLS\n";

    static const std::string_view truename_text =
        "Returns the fully qualified path of a file.\n"
        "\n"
        "TRUENAME [drive:][path]filename\n";

    static const std::string_view loadhigh_text =
        "Loads a program into the upper memory area.\n"
        "\n"
        "LOADHIGH [drive:][path]filename [parameters]\n"
        "LOADHIGH [/L:region1[,minsize1][;region2[,minsize2]...] [/S]]\n"
        "         [drive:][path]filename [parameters]\n"
        "\n"
        "  /L:region1[,minsize1][;region2[,minsize2]]...\n"
        "            Specifies the region(s) of memory into which to load\n"
        "            the program.\n"
        "  /S          Shrinks a UMB to its minimum size while the program\n"
        "              is loading.\n"
        "  [drive:][path]filename\n"
        "            Specifies the location and name of the program.\n"
        "  parameters  Specifies any command-line information required by\n"
        "              the program.\n";

    static const std::string_view lfnfor_text =
        "Enables/Disables Long file names when processing FOR commands.\n"
        "\n"
        "LFNFOR [ON | OFF]\n"
        "\n"
        "Type LFNFOR without a parameter to display the current setting.\n";

    static const std::string_view command_text =
        "Starts a new copy of the Windows Command Interpreter.\n"
        "\n"
        "COMMAND [[drive:]path] [device] [/E:nnnnn] [/L:nnnn] [/U:nnn] [/P] [/MSG]\n"
        "                       [/LOW] [/Y [/[C|K] command]]\n"
        "\n"
        "  [drive:]path    Specifies the directory containing COMMAND.COM.\n"
        "  device          Specifies the device to use for command input and output.\n"
        "  /E:nnnnn        Sets the initial environment size to nnnnn bytes.\n"
        "  /L:nnnn         Specifies internal buffers length (requires /P as well).\n"
        "  /U:nnn          Specifies the input buffer length (requires /P as well).\n"
        "  /P              Makes the new Command Interpreter permanent (can't exit).\n"
        "  /MSG            Stores all error messages in memory (requires /P as well).\n"
        "  /LOW            Forces COMMAND to keep its resident data in low memory.\n"
        "  /Y              Steps through the batch program specified by /C or /K.\n"
        "  /C command      Executes the specified command and returns.\n"
        "  /K command      Executes the specified command and continues running.\n";

    if (key == "DIR") return dir_text;
    if (key == "CALL") return call_text;
    if (key == "CHCP") return chcp_text;
    if (key == "RENAME") return rename_text;
    if (key == "DEL") return del_text;
    if (key == "TYPE") return type_text;
    if (key == "REM") return rem_text;
    if (key == "COPY") return copy_text;
    if (key == "PAUSE") return pause_text;
    if (key == "DATE") return date_text;
    if (key == "TIME") return time_text;
    if (key == "VER") return ver_text;
    if (key == "VOL") return vol_text;
    if (key == "CHDIR") return chdir_text;
    if (key == "MKDIR") return mkdir_text;
    if (key == "RMDIR") return rmdir_text;
    if (key == "BREAK") return break_text;
    if (key == "VERIFY") return verify_text;
    if (key == "SET") return set_text;
    if (key == "PROMPT") return prompt_text;
    if (key == "PATH") return path_text;
    if (key == "EXIT") return exit_text;
    if (key == "CTTY") return ctty_text;
    if (key == "ECHO") return echo_text;
    if (key == "GOTO") return goto_text;
    if (key == "SHIFT") return shift_text;
    if (key == "IF") return if_text;
    if (key == "FOR") return for_text;
    if (key == "CLS") return cls_text;
    if (key == "TRUENAME") return truename_text;
    if (key == "LOADHIGH") return loadhigh_text;
    if (key == "LFNFOR") return lfnfor_text;
    if (key == "COMMAND") return command_text;
    return {};
}

} // namespace cmdpp
