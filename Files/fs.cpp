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
    std::string name = this->getFile(filepath);
    if (name.length() > 55)
    {
        std::cout << "ERROR: Name too long\n";
        return 0;
    }
    dir_entry dir[64];
    int dirFatId;
    if(this->getDirectory(filepath, dir, dirFatId) == -1)
    {
        return 0;
    }
    for (int i = 0; i < numbEnteries(dir); i++)
    {
        if (dir[i].file_name == name)
        {
            std::cout << "ERROR: File already exists\n";
            return 0;
        }
    }
    int index = this->numbEnteries(dir);
    strcpy(dir[index].file_name, name.c_str());
    dir[index].type = TYPE_FILE;
    dir[index].access_rights = READWRITE;

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
    dir[index].size = fileSize;
    dir[index].first_blk = block;
    disk.write(FAT_BLOCK, (uint8_t*)fat);
    std::cout << "fat id: " << dirFatId << "\n";
    disk.write(dirFatId, (uint8_t*) dir);
    if(currentBlock == dirFatId)
    {
        std::cout << "updated wd\n";
        disk.read(currentBlock, (uint8_t*)this->workingDirectory);
    }
    return 0;
}

// cat <filepath> reads the content of a file and prints it on the screen
int
FS::cat(std::string filepath)
{
    bool found = false, rights = false;
    std::string fileText;

    for (int i = 0; i < numbEnteries(this->workingDirectory); i++) // DOES NOT WORK WITH HIERARCHIES!
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
    std::cout << "Name\tType\tAccess\tSize\n";
    std::string name, type, access, size;
    uint32_t aRights;

    int numb = this->numbEnteries(this->workingDirectory);
    for (int i = 0; i < numb; i++)
    {
        if (this->workingDirectory[i].type == TYPE_EMPTY)
        {
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

    for (int i = 0; i < numbEnteries(this->workingDirectory); i++)
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
    int newIndex = numbEnteries(this->workingDirectory);
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
    int num = this->numbEnteries(this->workingDirectory);
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

    fat[freeFat] = FAT_EOF;
    dir_entry folder[64];
    this->makeDirBlock(folder);
    this->workingDirectory[num].first_blk = freeFat;
    folder[0].type = TYPE_DIR;
    folder[0].first_blk = this->currentBlock;
    std::string nname = "..";
    strcpy(folder[0].file_name, nname.c_str());

    this->writeDirToDisk(freeFat, folder);
    disk.write(FAT_BLOCK, (uint8_t*)fat);
    this->writeDirToDisk(this->currentBlock, this->workingDirectory);
    dir_entry test[64];
    int np;
    this->readDirBlock(freeFat, test, np);
    return 0;
}

// cd <dirpath> changes the current (working) directory to the directory named <dirpath>
int
FS::cd(std::string dirpath)
{
    dir_entry dir[64];
    if(this->getDirectory(dirpath, dir, this->currentBlock,true) == -1)
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

int FS::numbEnteries(dir_entry* dir)
{
    int nr = 0;
    for (int i = 0; i < 64; i++)
    {
        if(dir[i].type != TYPE_EMPTY)
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

int FS::getDirectory(std::string path, dir_entry* dir, int& newBlock, bool cd)
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

    if(cd)
    { 
        directories.push_back(directory);
    }

    dir_entry dirs[64];
    for (int i = 0; i < 64; i++)
    {
        dirs[i] = workingDirectory[i];
        newBlock = this->currentBlock;
    }

    //If the file is in the same directory
    if (directories.size() == 0)
    {
        std::cout << "sending back wd\n";
    for (int i = 0; i < 64; i++)
        {
            dir[i] = workingDirectory[i];
            newBlock = this->currentBlock;
        }
        return 1;
    }

    bool found = false;
    int count = this->numbEnteries(this->workingDirectory);
    for (int i = 0; i < directories.size(); i++)
    {
        found = false;
        for (int j = 0; j < count; j++)
        {
            if(strcmp(dirs[j].file_name, directories[i].c_str()) == 0 && dirs[j].type == TYPE_DIR)
            {
                newBlock = dirs[j].first_blk;
                disk.read(dirs[j].first_blk, (uint8_t*)dirs);
                for (int i = 0; i < 64; i++)
                {
                    dir[i] = dirs[i];
                }
                
                found = true;
                count = 64;
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