#include <iostream>
#include "fs.h"

void FS::readDirBlock(int block, dir_entry *in, int& numbBlocks)
{
    disk.read(block, (uint8_t*) in);

    for (int i = 0; i < 64; i++)
    {
        if(in[i].type != TYPE_EMPTY)
        {
            numbBlocks++;
        }
    }
}

void FS::writeDirToDisk(int block, dir_entry *in)
{
    disk.write(block, (uint8_t*)in);
}

void FS::makeDirBlock(dir_entry *in, int numbBlocks, int startIndex)
{
    for (int i = startIndex; i < numbBlocks; i++)
    {
        in[i].type = TYPE_EMPTY;
    }
}

FS::FS()
{
    std::cout << "FS::FS()... Creating file system\n";
    disk.read(FAT_BLOCK, (uint8_t*)fat);

    if(fat[ROOT_BLOCK] != FAT_EOF || true) // no saved FS so make a new start
    {
        makeDirBlock(this->workingDirectory);
        writeDirToDisk(ROOT_BLOCK, this->workingDirectory);
        this->format();
    }
}

FS::~FS()
{
    std::cout <<  "I made it here \n";
}

// formats the disk, i.e., creates an empty file system
int
FS::format()
{
    std::cout << "FS::format()\n";
    for (int i = 2; i < BLOCK_SIZE/2; i++)
    {
        fat[i] = FAT_FREE;
    }
    fat[FAT_BLOCK] = FAT_EOF;
    fat[ROOT_BLOCK] = FAT_EOF;
    disk.write(FAT_BLOCK, (uint8_t*)fat);
    makeDirBlock(this->workingDirectory);
    return 0;
}

// create <filepath> creates a new file on the disk, the data content is
// written on the following rows (ended with an empty row)
int
FS::create(std::string filepath)
{
    if (filepath.length() > 55)
    {
        std::cout << "ERROR: Name too long\n";
        return 0;
    }

    for (int i = 0; i < numbEnteries(); i++)
    {
        if (workingDirectory[i].file_name == filepath)
        {
            std::cout << "ERROR: File already exists\n";
            return 0;
        }
    }
    int index = this->numbEnteries();
    strcpy(this->workingDirectory[index].file_name, filepath.c_str());
    this->workingDirectory[index].type = TYPE_FILE;
    this->workingDirectory[index].access_rights = READWRITE;

    bool done = false;
    std::string inputText;
    std::string text;

    while (!done)
    {
        std::getline(std::cin, inputText);
        if (inputText == "")
        {
            done = true;
        }
        else
        {
            inputText += '\n';
            text.append(inputText);
        }
    }

    int fileSize = text.size();
    int block = -1;
    this->writeToDisk(text, fileSize, block, true);
    this->workingDirectory[index].size = fileSize;
    this->workingDirectory[index].first_blk = block;
    disk.write(FAT_BLOCK, (uint8_t*)fat);
    disk.write(ROOT_BLOCK, (uint8_t*) workingDirectory);
    return 0;
}

// cat <filepath> reads the content of a file and prints it on the screen
int
FS::cat(std::string filepath)
{
    bool found = false, rights = false;
    std::string fileText;

    for (int i = 0; i < numbEnteries(); i++) // DOES NOT WORK WITH HIERARCHIES!
    {
        if (workingDirectory[i].file_name == filepath)
        {
            uint8_t access = workingDirectory[i].access_rights;
            if (access == READ || access == READWRITE || access == 0x07)
            {
                rights = true;
                readFromDisk(fileText, i);
                std::cout << fileText;
            }

            found = true;
            break;
        }
    }

    if (!found)
    {
        std::cout << "ERROR: File not found\n";
    }
    else if (!rights)
    {
        std::cout << "ERROR: You do not have access to this file\n";
    }
    return 0;
}

// ls lists the content in the currect directory (files and sub-directories)
int
FS::ls()
{
    int numb = this->numbEnteries();
    std::cout << "WD has: " << numb << " number of enteries\n";
    std::cout << "Name\tType\tAccess\tSize\n";
    std::string name, type, access, size;
    uint32_t aRights;

    for (int i = 0; i < numb; i++)
    {
        if (this->workingDirectory[i].type == TYPE_EMPTY)
        {
            std::cout << "ah shit here we go again\n";
            continue;
        }
        
        name = this->workingDirectory[i].file_name;
        size = std::to_string(this->workingDirectory[i].size);
        aRights = this->workingDirectory[i].access_rights;
        if (this->workingDirectory[i].type == TYPE_FILE)
        {
            type = "File";
        }
        else
        {
            type = "Dir";
            size = "-";
        }
        std::cout << name << "\t" << type << "\t" << access << "\t" << size << "\n";
    }

    return 0;
}

// cp <sourcepath> <destpath> makes an exact copy of the file
// <sourcepath> to a new file <destpath>
int
FS::cp(std::string sourcepath, std::string destpath)
{
    int index = -1;
    std::string fileText;

    for (int i = 0; i < numbEnteries(); i++)
    {
        if (workingDirectory[i].file_name == sourcepath)
        {
            index = i;
        }

        if (workingDirectory[i].file_name == destpath)
        {
            std::cout << "ERROR: File already exists\n";
            return 0;
        }
    }

    if (index == -1)
    {
        std::cout << "ERROR: Could not find file\n";
        return 0;
    }

    //Reads in the text if you have access to the file
    int accessRight = workingDirectory[index].access_rights;
    if (accessRight == READ || accessRight == 0x06 || accessRight == 0x07)
    {
        readFromDisk(fileText, index);
    }
    else
    {
        std::cout << "ERROR: Access denied\n";
        return 0;
    }

    //Creating the new file for workingdirectory
    int newIndex = numbEnteries();
    int block = -1;

    strcpy(workingDirectory[newIndex].file_name, destpath.c_str());
    workingDirectory[newIndex].size = workingDirectory[index].size;
    workingDirectory[newIndex].type = workingDirectory[index].type;
    workingDirectory[newIndex].access_rights = workingDirectory[index].access_rights;
    writeToDisk(fileText, workingDirectory[newIndex].size, block, true);
    workingDirectory[newIndex].first_blk = block;
    
    disk.write(FAT_BLOCK, (uint8_t*)fat);
    return 0;
}

// mv <sourcepath> <destpath> renames the file <sourcepath> to the name <destpath>,
// or moves the file <sourcepath> to the directory <destpath> (if dest is a directory)
int
FS::mv(std::string sourcepath, std::string destpath)
{
    std::cout << "FS::mv(" << sourcepath << "," << destpath << ")\n";
    return 0;
}

// rm <filepath> removes / deletes the file <filepath>
int
FS::rm(std::string filepath)
{
    std::cout << "FS::rm(" << filepath << ")\n";
    return 0;
}

// append <filepath1> <filepath2> appends the contents of file <filepath1> to
// the end of file <filepath2>. The file <filepath1> is unchanged.
int
FS::append(std::string filepath1, std::string filepath2)
{
    std::cout << "FS::append(" << filepath1 << "," << filepath2 << ")\n";
    return 0;
}

// mkdir <dirpath> creates a new sub-directory with the name <dirpath>
// in the current directory
int
FS::mkdir(std::string dirpath)
{
    std::string name = this->getFile(dirpath);
    int num = this->numbEnteries();
    if(num > 64)
    {
        std::cout << "ERROR: Directory full\n";
        return 0;
    }
    for (int i = 0; i < num; i++)
    {
        if(workingDirectory[i].file_name == name.c_str())
        {
            std::cout << "ERROR: Name exists\n";
            return 0;
        }
    }
    
    this->workingDirectory[num].type = TYPE_DIR;
    this->workingDirectory[num].access_rights = READWRITE;
    strcpy(this->workingDirectory[num].file_name, name.c_str());

    int freeFat = -1;
    for (int i = 2; i < BLOCK_SIZE/2; i++)
    {
        if(fat[i] == FAT_FREE)
        {
            freeFat = i;
            break;
        }
    }

    std::cout << "The fat block is: " << freeFat << "\n";
    
    dir_entry folder[64];
    this->makeDirBlock(folder);
    this->workingDirectory[num].first_blk = freeFat;
    folder[0].type = TYPE_DIR;
    folder[0].first_blk = this->currentBlock;
    std::string nname = "..";
    strcpy(folder[0].file_name, nname.c_str());
    for (int i = 0; i < 64; i++)
    {
      //  std::cout << "spot: " << i << " has: " << (int)folder[i].type << "\n";
    }
    
    this->writeDirToDisk(freeFat, folder);
    this->writeDirToDisk(ROOT_BLOCK, this->workingDirectory);
    return 0;
}

// cd <dirpath> changes the current (working) directory to the directory named <dirpath>
int
FS::cd(std::string dirpath)
{
    dir_entry dir[64];
    if(this->getDirectory(dirpath, dir) == -1)
    {
        return 0;
    }
    for (int i = 0; i < 64; i++)
    {
        this->workingDirectory[i] = dir[i];
    }
    return 0;
}

// pwd prints the full path, i.e., from the root directory, to the current
// directory, including the currect directory name
int
FS::pwd()
{
    std::cout << "FS::pwd()\n";
    return 0;
}

// chmod <accessrights> <filepath> changes the access rights for the
// file <filepath> to <accessrights>.
int
FS::chmod(std::string accessrights, std::string filepath)
{
    std::cout << "FS::chmod(" << accessrights << "," << filepath << ")\n";
    return 0;
}

void FS::writeToDisk(std::string fileText, int fileSize, int &FirstBlock, bool firstAdd)
{
    std::string fixedText;
    int lastBlock, count = 0;

    for (int i = 2; i < BLOCK_SIZE/2; i++)
    {
        if (fat[i] == FAT_FREE)
        {
            if (firstAdd)
            {
                FirstBlock = i;
                fixedText = fileText.substr(4096 * count, 4096);
                disk.write(i, (uint8_t*)fixedText.c_str());

                count++;
                firstAdd = true;
            }
            else
            {
                //Adding to new disk block if file was too big
                fat[lastBlock] = i;
                fixedText = fileText.substr(4096 * count, 4096);
                disk.write(i, (uint8_t*)fixedText.c_str());
                count++;
            }

            //Checking if file is bigger than disk block
            fileSize -= 4096;

            if (fileSize <= 0)
            {
                fat[i] = FAT_EOF;
                break;
            }

            lastBlock = i;
        }
    }
}

//Reads from the disk
void FS::readFromDisk(std::string& fileText, int fileIndex)
{
    char* buffer;
    int lastPlace = workingDirectory[fileIndex].first_blk;

    while (lastPlace != FAT_EOF)
    {
        buffer = new char[BLOCK_SIZE];
        disk.read(lastPlace, (uint8_t*)buffer);
        fileText.append(buffer);
        delete[] buffer;
        lastPlace = fat[lastPlace];
    }
}

int FS::numbEnteries()
{
    int nr = 0;
    for (int i = 0; i < 64; i++)
    {
        if(this->workingDirectory[i].type != TYPE_EMPTY)
        {
            nr++;
        }
    }
    return nr;
}

int FS::firstFreeEnterie()
{
    int free = 0;
    for (int i = 0; i < 64; i++)
    {
        if(this->workingDirectory[i].type != TYPE_EMPTY)
        {
            free = i;
            return free;
        }
    }
    return free;
}

std::string FS::getFile(std::string path)
{
    //Only looking for the filename
    char text[path.size()];
    strcpy(text, path.c_str());
    std::vector <std::string> directories;
    std::string directory;

    if (path.size() == 1 && path[0] == '/')
    {
        directory = path[0];
        return directory;
    }

    for (int i = 0; i < path.size(); i++)
    {
        if (i == 0 && text[i] == '/')
        {
            continue;
        }
        else if (i > 0 && text[i] == '/')
        {
            directories.push_back(directory);
            directory = "";
        }
        else
        {
            directory += text[i];
        }
    }

    directories.push_back(directory);
    directory = directories[directories.size() - 1];
    return directory;
}

int FS::getDirectory(std::string path, dir_entry* dir)
{
    //Dividing the path into strings
    char text[path.size()];
    strcpy(text, path.c_str());
    std::vector <std::string> directories;
    std::string directory;

    for (int i = 0; i < path.size(); i++)
    {
        if (i == 0 && text[i] == '/')
        {
            directory = text[i];
            directories.push_back(directory);
            directory = "";
        }
        else if (i > 0 && text[i] == '/')
        {
            directories.push_back(directory);
            directory = "";
        }
        else
        {
            directory += text[i];
        }
    }
    directories.push_back(directory);


    //If the file is in the same directory
    if (directories.size() == 0)
    {
        dir = workingDirectory;
        return 1;
    }

    dir_entry* dirs = this->workingDirectory;
    bool found = false;
    int count = this->numbEnteries();
    for (int i = 0; i < directories.size(); i++)
    {
        found = false;
        for (int j = 0; j < count; j++)
        {
            if(strcmp(dirs[j].file_name, directories[i].c_str()) == 0 && dirs[j].type == TYPE_DIR)
            {
                std::cout << "moving to block: " << dirs[j].first_blk << "\n";
                disk.read(dirs[j].first_blk, (uint8_t*)dir);
                found = true;
                count = numbEnteries();
                std::cout << "new nr is: " << count << "\n";
                break;
            }
        }
        if (!found)
        {
            std::cout << "ERROR: No dir found\n";
            return-1;
        }
    }
    return 1;
}