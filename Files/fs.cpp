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
    this->numbEnteries = 0;
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
    std::cout << "FS::create(" << filepath << ")\n";
    strcpy(this->workingDirectory[this->numbEnteries].file_name, filepath.c_str());
    this->workingDirectory[this->numbEnteries].type = TYPE_FILE;
    this->workingDirectory[this->numbEnteries].size = 10;
    this->numbEnteries++;
    return 0;
}

// cat <filepath> reads the content of a file and prints it on the screen
int
FS::cat(std::string filepath)
{
    std::cout << "FS::cat(" << filepath << ")\n";
    return 0;
}

// ls lists the content in the currect directory (files and sub-directories)
int
FS::ls()
{
    std::cout << "FS::ls()\n";
    std::cout << "Name\tType\tAccess\tSize\n";
    std::string name, type, access, size;
    uint32_t aRights;

    for (int i = 0; i < this->numbEnteries; i++)
    {
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
    std::cout << "FS::cp(" << sourcepath << "," << destpath << ")\n";
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
    std::cout << "FS::mkdir(" << dirpath << ")\n";
    return 0;
}

// cd <dirpath> changes the current (working) directory to the directory named <dirpath>
int
FS::cd(std::string dirpath)
{
    std::cout << "FS::cd(" << dirpath << ")\n";
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
