/*
	Name: Brandon Carter
	ID: 1001350607
	NETID: bxc0607
*/

//#define _GNU_SOURCE

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <sys/stat.h>

#define WHITESPACE " \t\n"      // We want to split our command line up into tokens
// so we need to define what delimits our tokens.
// In this case  white space
// will separate the tokens on our command line

#define MAX_COMMAND_SIZE 255    // The maximum command-line size

#define MAX_NUM_ARGUMENTS 5     // Mav shell only supports five arguments

/*
    Structure that represents some information about the file system.
*/
struct FileSystemAttr
{
    int16_t BPB_BytesPerSec;
    int8_t BPB_SecPerClus;
    int16_t BPB_RsvdSecCnt;
    int8_t BPB_NumFATs;
    int32_t BPB_FATSz32;
    char BS_VolLab[11];
};

/*
    Records can be represented by this structure.
*/
struct __attribute__((__packed__)) DirectoryEntry
{
    char DIR_Name[11];
    uint8_t DIR_Attr;
    uint8_t Unused1[8];
    uint16_t DIR_FirstClusterHigh;
    uint8_t Unused2[4];
    uint16_t DIR_FirstClusterLow;
    uint32_t DIR_FileSize;
};

/*
    Open file img specified by the user or report error.
*/
int fileOpener(char *token[], FILE **fp)
{
    struct stat temp;
    if(*fp != NULL)
    {
        //if fp is not NULL, file is already opened
        printf("Error: File system image already open.\n");
        return 0;
    }
    //check if the file img trying to be opened can be found
    else if(stat(token[1],&temp) == -1)
    {
        //print error if file cannot be found
        printf("Error: File system image not found.\n");
        return 0;
    }
    else
    {
        //open file user requested
        *fp = fopen(token[1],"r");
        return 1;
    }
}

/*
    Display file system information for the info command.
*/
void fileSysSpecDisplayer(struct FileSystemAttr *info)
{
    //display the information
    printf("BPB_BytesPerSec : %d\n",info->BPB_BytesPerSec);
    printf("BPB_BytesPerSec : %x\n\n",info->BPB_BytesPerSec);

    printf("BPB_SecPerClus : %d\n",info->BPB_SecPerClus);
    printf("BPB_SecPerClus : %x\n\n",info->BPB_SecPerClus);

    printf("BPB_RsvdSecCnt : %d\n",info->BPB_RsvdSecCnt);
    printf("BPB_RsvdSecCnt : %x\n\n",info->BPB_RsvdSecCnt);
    
    printf("BPB_NumFATS : %d\n",info->BPB_NumFATs);
    printf("BPB_NumFATS : %x\n\n",info->BPB_NumFATs);

    printf("BPB_FATSz32 : %d\n",info->BPB_FATSz32);
    printf("BPB_FATSz32 : %x\n\n",info->BPB_FATSz32);
}

/*
    Set the file system specs
*/
void setFileSysSpecs(FILE **fp, struct FileSystemAttr *info)
{
    //get information about bytes per sector
    short bytes;
    fseek(*fp, 11, SEEK_SET);
    fread(&bytes, 1, 2, *fp);
    info->BPB_BytesPerSec = bytes;

    //get information about sectors per cluster
    short sectors;
    fseek(*fp, 13, SEEK_SET);
    fread(&sectors, 1, 1, *fp);
    info->BPB_SecPerClus = sectors;

    //get information about number of reserved sectors
    short rsvdSectors;
    fseek(*fp, 14, SEEK_SET);
    fread(&rsvdSectors, 1, 2, *fp);
    info->BPB_RsvdSecCnt = rsvdSectors;

    //get information about number of FATs
    short numFats;
    fseek(*fp, 16, SEEK_SET);
    fread(&numFats, 1, 1, *fp);
    info->BPB_NumFATs = numFats;

    //get information about FAT size count
    short fatsz;
    fseek(*fp, 36, SEEK_SET);
    fread(&fatsz, 1, 4, *fp);
    info->BPB_FATSz32 = fatsz;

    //get information about volume name
    char volumeName[11];
    fseek(*fp, 71, SEEK_SET);
    fread(&volumeName, 1, 11, *fp);
    strcpy(info->BS_VolLab,volumeName);
}

/*
    For the ls command. Display contents of directory.
*/
void listDirectory(FILE **fp, struct DirectoryEntry dir[])
{
    int i = 0;
    //begin the loop through current directory entry
    for(i = 0; i < 16; i++)
    {
        //printf("%c : attr = %u\n",dir[i].DIR_Name[0],(unsigned int)dir[i].DIR_Attr);
        //printf("%u\n\n",(unsigned int)dir[i].DIR_Name[0]);
        //only want to print files that have correct attributes so check it here
        if((dir[i].DIR_Attr == 1 || dir[i].DIR_Attr == 16 || dir[i].DIR_Attr == 32 || dir[i].DIR_Attr == 48) 
        && 
        ((unsigned int)dir[i].DIR_Name[0] != 0xffffffe5)) 
        {
            //there is no \0 to terminate dir_name so ensure only 11 characters are printed
            printf("%.*s\n",11,dir[i].DIR_Name);
        }
    }
}

/*
    Finds the starting address of a block of data given the sector number corresponding to that
    data block.
*/
int LBAToOffset(int32_t sector, struct FileSystemAttr *info)
{
    return ((sector - 2) * info->BPB_BytesPerSec) + (info->BPB_BytesPerSec * info->BPB_RsvdSecCnt) 
    + (info->BPB_NumFATs * info->BPB_FATSz32 * info->BPB_BytesPerSec);
}

/*
    Set the values for the directory structure inside the directory entry array.
*/
void setDirectory(FILE **fp, struct DirectoryEntry dir[], int *filePointer)
{
    int i;
    for(i = 0; i < 16; i++)
        {
            //printf("before: %u\n",dir[i].DIR_Attr);
            fread(&dir[i],1,32,*fp);
            //printf("after: %u\n",dir[i].DIR_Attr);
            *filePointer += 32;
        }
}

/*
    Given a directory name within the current directory, this function returns the address
    of its clusters/data blocks.
*/
int findAddress(char *dirNameToken, struct DirectoryEntry dir[], struct FileSystemAttr *info)
{
    int i, j = 0, address = -1;
    //loop through directory entry array and find the directory name the user entered
    for(i = 0; i < 16; i++)
    {
        char *realDirName;
        j = 0;
        while(dir[i].DIR_Name[j] != '\0' && j != 12)
        {
            if(dir[i].DIR_Name[j] == ' ')
            {
                realDirName = (char*)malloc(sizeof(char)*j);
                strncpy(realDirName,dir[i].DIR_Name,j);
                break;
            }
            j++;
        }
        
        //if the directory name the user entered matches directory name i
        if(strcmp(realDirName,dirNameToken) == 0 && dir[i].DIR_Attr == 0x10)
        {
            //return file pointer address to the directory the user is switching to
            address = LBAToOffset(dir[i].DIR_FirstClusterLow,&(*info));
            if(dir[i].DIR_FirstClusterLow == 0)
            {
                address = (info->BPB_NumFATs * info->BPB_FATSz32 * info->BPB_BytesPerSec) + 
                (info->BPB_RsvdSecCnt * info->BPB_BytesPerSec);
            }
            break;
        }
    }
    return address;
}

/*
    Allow user to change their directory with the cd command.
*/
void changeDirectory(char* dirName, FILE **fp, struct DirectoryEntry dir[], int *filePointer, 
struct FileSystemAttr *info)
{
    //if the command is cd and nothing follows, go back to root
    if(dirName == NULL)
    {
        *filePointer = (info->BPB_NumFATs * info->BPB_FATSz32 * info->BPB_BytesPerSec) + 
        (info->BPB_RsvdSecCnt * info->BPB_BytesPerSec);
        fseek(*fp, *filePointer, SEEK_SET);
        setDirectory(&(*fp),dir,&(*filePointer));
        return;
    }

    //convert to uppercase
    int i = 0;
    while(dirName[i] != '\0')
    {
        dirName[i] = toupper(dirName[i]);
        i++;
    }
    //i represents where we are in the string 
    i = 0;
    //j represents the start index of the directory name in the string
    int j = 0;
    //count represents the length of the directory name
    int count = 0;

    /*
        dirName is the entire string inputed by the user following cd.
        for example, for the command: "cd test/hello/project/", the file path
        would be stored into dirName and the process is to go through it
        character by character, when we hit a '/' we process the string up to '/'
        and find the address of that directory. then we repeat the process until
        completion. 
    */
    while(dirName[i] != '\0')
    {
        //if character we are looking at is a '/'
        if(dirName[i] == '/')
        {
            //create token string which saves directory name up to '/'
            char token[count+1];
            //copy the name from the file path into token so it's separated
            strncpy(token,dirName + j,i-j);
            //null terminate the string
            token[count] = '\0';
            //re assign the index
            j = i + 1;
            count = 0;
            i++;

            //find the address/cluster for that directory name
            *filePointer = findAddress(token,dir,&(*info));

            if(*filePointer == -1)
            {
                printf("cd: %s: No such file or directory\n",dirName);
                return;
            }

            //reassign the file pointer to it
            fseek(*fp, *filePointer, SEEK_SET);
            //assign the current directory structure to the directory we just processed
            setDirectory(&(*fp),dir,&(*filePointer));
            //continue the loop
            
            continue;
        }
        count++;
        i++;
    }

    //same as above but for the last string which is simply at the end
    char token[count+1];
    strncpy(token,dirName + j,(i+1)-j);
    *filePointer = findAddress(token,dir,&(*info));
    if(*filePointer == -1)
    {
        printf("cd: %s: No such file or directory\n",dirName);
        return;
    }
    fseek(*fp, *filePointer, SEEK_SET);
    setDirectory(&(*fp),dir,&(*filePointer));
}

/*
    Given a file name, returns the index of where it's located in the directoryentry array.
*/
int findFile(char *fileName, struct DirectoryEntry dir[])
{
    //convert the file name the user entered to upper case so matching is not case sensitive
    int i = 0;
    while(fileName[i] != '\0')
    {
        fileName[i] = toupper(fileName[i]);
        i++;
    }
    
    //user typed file name too long
    if(i > 12)
    {
        return -1;
    }
    
    //copy what user entered into fixed sized array for easy comparison
    char usersFileName[14] = {0};
    strncpy(usersFileName,fileName,i);
    
    //loop through directoryentry array to find match
    for(i = 0; i < 16; i++)
    {
        //newfilename will hold the directory entry and parse it for comparison to what the user entered
        char newFileName[14] = {0};
        char dirNameNulled[12] = {0};
        char extension[4] = {0};
        //dir name nulled will hold the directory name with nulls at the end
        strncpy(dirNameNulled,dir[i].DIR_Name,11);
        int j = 0;
        for(j = 0; j < 8; j++)
        {
            //find out where the name ends by finding the first space in the name
            if(dirNameNulled[j] == ' ')
            {
                break;
            }
        }
        //null the character after the name
        dirNameNulled[j] = '\0';
        //copy it into newFileName so it contains just the name with no garbage character
        strncpy(newFileName,dirNameNulled,j);
        //if there's an extension at the end of the name, copy it and append it to the end
        if(dir[i].DIR_Name[8] != ' ')
        {
            strncpy(extension,dir[i].DIR_Name + 8,3);
            strcat(newFileName,".");
            strcat(newFileName,extension);
        }
        //compare the names for a match
        if(strcmp(usersFileName,newFileName) == 0)
        {
            return i;
        }
    }
    
    
    
    return -1;
}

/*
    Shall print the attributes and starting cluster number of the file or directory name.
*/
void displayFileAttr(char* fileName, struct DirectoryEntry dir[])
{
    //determine index of the file the user wants
    int i = findFile(fileName, dir);

    if(i == -1)
    {
        //if we reach this point, the file does not exist
        printf("Error: File not found\n");
        return;
    }

    //print attributes
    printf("Attribute\tSize\tStarting Cluster Number\n");
    printf("%u\t\t%u\t%u\n",dir[i].DIR_Attr,dir[i].DIR_FileSize,dir[i].DIR_FirstClusterLow);
}

/*
    Given a logical block address, look up into the first FAT, and return the logical block address
    of the block in the file. If there is no further blocks then return -1.
*/
int16_t NextLB(uint32_t sector, struct FileSystemAttr *info, FILE **fp)
{
    uint32_t FATAddress = (info->BPB_BytesPerSec * info->BPB_RsvdSecCnt) + (sector * 4);
    int16_t val;
    printf("FATADDRESS: %u\n",FATAddress);
    fseek(*fp, FATAddress, SEEK_SET);
    fread(&val, 2, 1, *fp);
    return val;
}

/*
    Reads from the given file at the position, in bytes, specified by the position 
    parameter and output the number of bytes specified.
*/
void readFile(char *token[], FILE **fp, int *filePointer, struct DirectoryEntry dir[], 
struct FileSystemAttr *info)
{
    //if file name not provided, print error message
    if(token[1] == NULL)
    {
        printf("Error: File name was not provided\n");
        return;
    }
    //if number of bytes is not valid, print error message
    else if(token[2] == NULL || token[3] == NULL)
    {
        printf("Enter a valid number of bytes to read.\n");
        return;
    }

    //store filename entered by user
    char filename[strlen(token[1])];
    strcpy(filename,token[1]);

    //convert position entered by user into an int
    char charPosition[strlen(token[2])];
    strcpy(charPosition,token[2]);
    int position = atoi(charPosition);

    //convert number of bytes entered by user into an int
    char charBytes[strlen(token[3])];
    strcpy(charBytes,token[3]);
    int numberOfBytes = atoi(charBytes);

    //find index of file in directoryEntry array and determine address of that file
    int index = findFile(filename,dir);
    
    //file was not found
    if(index == -1)
    {
        printf("Error: File not found\n");
        return;
    }
    
    int address = LBAToOffset(dir[index].DIR_FirstClusterLow,&(*info)) + position;

    //print the bytes of the file the user requested
    fseek(*fp, address, SEEK_SET);
    int i;
    for(i = 0; i < numberOfBytes; i++)
    {
        short val;
        //read one byte at a time and print the value in hex
        fread(&val, 1, 1, *fp);
        printf("%x ",val);
    }
    printf("\n");
}

void getFile(char* fileName, FILE **fp, struct DirectoryEntry dir[], struct FileSystemAttr *info)
{
    //find index of file in directoryEntry array and determine address of that file
    int index = findFile(fileName,dir);
    int address = LBAToOffset(dir[index].DIR_FirstClusterLow,&(*info));

    if(index == -1)
    {
        printf("Error: File not found\n");
        return;
    }

    //create new file pointer for file being written to our local computer
    FILE *newFP;
    newFP = fopen(fileName,"w");

    //set file pointer in the file we want to download
    fseek(*fp, address, SEEK_SET);
    int i;
    //loop through file and write every byte to the file on our local computer
    for(i = 0; i < dir[index].DIR_FileSize; i++)
    {
        char val[1];
        //read one byte at a time and write the value
        fread(&val, 1, 1, *fp);
        fwrite(val, 1, sizeof(val), newFP);
    }
    fclose(newFP);
}

/*
    Print the volume name.
*/
void printVolName(struct FileSystemAttr *info)
{
    //if volume name not null, print it (stored in file sys attr struct)
    if(info->BPB_RsvdSecCnt != 0)
    {
        printf("Volume name of the file is '%s'\n",info->BS_VolLab);
    }
    else
    {
        //doesn't exist, print error message
        printf("Error: volume name not found.\n");
    }
}

/*
    Parse command entered by user to determine what action to take.
*/
void commandParser(char *token[], FILE **fp, int *filePointer, struct FileSystemAttr *info, 
struct DirectoryEntry dir[])
{   
    //if command entered is open
    if(strcmp(token[0],"open") == 0)
    {
        //open the file
        if(fileOpener(token,&(*fp)) == 0)
        {
            return;
        }

        //set the basic specs for file system
        setFileSysSpecs(&(*fp),&(*info));

        //set up the file pointer
        *filePointer = (info->BPB_NumFATs * info->BPB_FATSz32 * info->BPB_BytesPerSec) + 
        (info->BPB_RsvdSecCnt * info->BPB_BytesPerSec);

        //set directory entry information
        fseek(*fp, *filePointer, SEEK_SET);
        setDirectory(&(*fp),dir,&(*filePointer));
    }
    //if command entered is close
    else if(strcmp(token[0],"close") == 0)
    {
        //if fclose returns an error, file must not be open, report an error
        if(*fp == NULL || fclose(*fp) != 0)
        {
            printf("Error: File system not open.\n");
        }
        else
        {
            *fp = NULL;
        }
    }
    //if command entered is info
    else if(strcmp(token[0],"info") == 0)
    {
        //if file isn't currently open, display error message
        if(*fp == NULL)
        {
            printf("Error: File system image must be opened first.\n");
        }
        else
        {
            fileSysSpecDisplayer(&(*info));
        }
    }
    //if command entered is ls
    else if(strcmp(token[0],"ls") == 0)
    {
        if(*fp == NULL)
        {
            printf("Error: File system image must be opened first.\n");
        }
        else
        {
            listDirectory(&(*fp),dir);
        }
    }
    //if command entered is cd
    else if(strcmp(token[0],"cd") == 0)
    {
        if(*fp == NULL)
        {
            printf("Error: File system image must be opened first.\n");
        }
        else
        {
            changeDirectory(token[1],&(*fp),dir,&(*filePointer),&(*info));
        }
    }
    //if command entered is stat
    else if(strcmp(token[0],"stat") == 0)
    {
        if(*fp == NULL)
        {
            printf("Error: File system image must be opened first.\n");
        }
        else
        {
            displayFileAttr(token[1],dir);
        }
    }
    //if command entered is read
    else if(strcmp(token[0],"read") == 0)
    {
        if(*fp == NULL)
        {
            printf("Error: File system image must be opened first.\n");
        }
        else
        {
            readFile(token,&(*fp),&(*filePointer),dir,&(*info));
        }
    }
    //if command entered is get
    else if(strcmp(token[0],"get") == 0)
    {
        if(*fp == NULL)
        {
            printf("Error: File system image must be opened first.\n");
        }
        else
        {
            getFile(token[1],&(*fp),dir,&(*info));
        }
    }
    //if command entered is volume
    else if(strcmp(token[0],"volume") == 0)
    {
        if(*fp == NULL)
        {
            printf("Error: File system image must be opened first.\n");
        }
        else
        {
            printVolName(&(*info));
        }
    }
    else
    {
        printf("Entered command is not supported by the terminal.\n");
    }
}

int main()
{
    char * cmd_str = (char*) malloc( MAX_COMMAND_SIZE );
    int filePointer = -1;
    struct FileSystemAttr info;
    struct DirectoryEntry dir[16];

    //initialize file pointer to NULL
    FILE *fp = NULL;
    
    while( 1 )
    {
        // Print out the msh prompt
        printf ("mfs> ");
        
        // Read the command from the commandline.  The
        // maximum command that will be read is MAX_COMMAND_SIZE
        // This while command will wait here until the user
        // inputs something since fgets returns NULL when there
        // is no input
        while( !fgets (cmd_str, MAX_COMMAND_SIZE, stdin) );
        
        /* Parse input */
        char *token[MAX_NUM_ARGUMENTS];
        
        int   token_count = 0;
        
        // Pointer to point to the token
        // parsed by strsep
        char *arg_ptr;
        
        char *working_str  = strdup( cmd_str );
        
        // we are going to move the working_str pointer so
        // keep track of its original value so we can deallocate
        // the correct amount at the end
        char *working_root = working_str;
        
        // Tokenize the input stringswith whitespace used as the delimiter
        while ( ( (arg_ptr = strsep(&working_str, WHITESPACE ) ) != NULL) &&
               (token_count<MAX_NUM_ARGUMENTS))
        {
            token[token_count] = strndup( arg_ptr, MAX_COMMAND_SIZE );
            if( strlen( token[token_count] ) == 0 )
            {
                token[token_count] = NULL;
            }
            token_count++;
        }

        if(token[0] == NULL)
        {
            continue;
        }

        commandParser(token,&fp,&filePointer,&info,dir);
    }
}
