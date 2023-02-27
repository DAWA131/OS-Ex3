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
    disk.read(FAT_BLOCK, (uint8_t*)fat);

    if(fat[ROOT_BLOCK] != FAT_EOF) // no saved FS so make a new start
    {
        makeDirBlock(this->workingDirectory);
        writeDirToDisk(ROOT_BLOCK, this->workingDirectory);
        this->format();
    }
    else
    {
        disk.read(ROOT_BLOCK, (uint8_t*)this->workingDirectory);
    }
}

FS::~FS()
{
}

// formats the disk, i.e., creates an empty file system
int
FS::format()
{
    disk.read(ROOT_BLOCK, (uint8_t*)this->workingDirectory);
    this->currentBlock = ROOT_BLOCK;
    this->makeDirBlock(this->workingDirectory);
    disk.write(ROOT_BLOCK, (uint8_t*)this->workingDirectory);
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
    if(this->numbEnteries(this->workingDirectory) > 63)
    {
        std::cout << "ERROR: dir is full\n";
        return 0;
    }
    std::string name = this->getFile(filepath);
    if (name.length() > 55)
    {
        std::cout << "ERROR: Name too long\n";
        return 0;
    }
    dir_entry dir[64];
    int dirFatId;
    if(this->getDirectory(filepath, dir, dirFatId, false) == -1)
    {
        std::cout << "ERROR: No dir found\n";
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
    disk.write(dirFatId, (uint8_t*) dir);
    if(currentBlock == dirFatId)
    {
        disk.read(currentBlock, (uint8_t*)this->workingDirectory);
    }
    return 0;
}

// cat <filepath> reads the content of a file and prints it on the screen
int
FS::cat(std::string filepath)
{
    bool found = false, rights = false;
    std::string fileText, file;
    dir_entry dir[64];
    int dirFatId;
    if(this->getDirectory(filepath, dir, dirFatId, false) == -1)
    {
        std::cout << "ERROR: No dir found\n";
        return 0;
    }
    file = getFile(filepath);

    for (int i = 0; i < numbEnteries(dir); i++)
    {
        if (dir[i].file_name == file)
        {
            if (dir[i].type == TYPE_DIR)
            {
                std::cout << "ERROR: Can't be a directory\n";
                return 0;
            }

            uint8_t access = dir[i].access_rights;
            if (access == READ || access == READWRITE || access == 0x07)
            {
                rights = true;
                readFromDisk(fileText, i, dir);
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
    for (int i = 0; i < 64; i++)
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
        
        switch (aRights)
        {
            case READ:
            access = "r--";
            break;

            case WRITE:
            access = "-w-";
            break;

            case EXECUTE:
            access = "--x";
            break;

            case 0x05:
            access = "r-x";
            break;

            case 0x06:
            access = "rw-";
            break;

            case 0x07:
            access = "rwx";
            break;
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
    dir_entry dir[64];
    dir_entry destDir[64];
    int dirFatId;
    if(this->getDirectory(sourcepath, dir, dirFatId, false) == -1)
    {
        std::cout << "ERROR: No dir found\n";
        return 0;
    }
    std::string file = getFile(sourcepath);

    if(this->getDirectory(destpath, destDir, dirFatId, false) == -1)
    {
        std::cout << "ERROR: No dir found\n";
        return 0;
    }
    std::string destFile = getFile(destpath);

    //Making sure the first file exists
    int index = -1;
    for (int i = 0; i < numbEnteries(dir); i++)
    {
        if (dir[i].file_name == file)
        {
            if (dir[i].type == TYPE_DIR)
            {
                std::cout << "ERROR: The first needs to be a file\n";
                return 0;
            }

            index = i;
            break;
        }
    }

    if (index == -1)
    {
        std::cout << "ERROR: Could not find file\n";
        return 0;
    }

    //Seeing if the destination is a file or directory
    bool directory = false;

    for (int i = 0; i < numbEnteries(destDir); i++)
    {
        if (destDir[i].file_name == destFile)
        {
            if (destDir[i].type == TYPE_FILE)
            {
                std::cout << "ERROR: File already exists\n";
                return 0;
            }
            else if (destDir[i].type == TYPE_DIR)
            {
                getDirectory(destpath, destDir, dirFatId, true);
                directory = true;
                break;
            }   
        }
    }

    if (directory)
    {
        for (int i = 0; i < numbEnteries(destDir); i++)
        {
            if (destDir[i].file_name == file)
            {
                std::cout << "ERROR: File already exists\n";
                return 0;
            }
        }
    }

    //Reads in the text if you have access to the file
    std::string fileText;
    int accessRight = dir[index].access_rights;
    if (accessRight == READ || accessRight == 0x06 || accessRight == 0x07)
    {
        readFromDisk(fileText, index, dir);
    }
    else
    {
        std::cout << "ERROR: Access denied\n";
        return 0;
    }

    //Creating the new file for workingdirectory
    int newIndex = numbEnteries(destDir);
    int block = -1;
    std::string name;

    if (directory) name = file;
    else name = destFile;

    strcpy(destDir[newIndex].file_name, name.c_str());
    destDir[newIndex].size = dir[index].size;
    destDir[newIndex].type = dir[index].type;
    destDir[newIndex].access_rights = dir[index].access_rights;
    writeToDisk(fileText, destDir[newIndex].size, block, true);
    destDir[newIndex].first_blk = block;

    disk.write(dirFatId, (uint8_t*)destDir);
    disk.write(FAT_BLOCK, (uint8_t*)fat);
    if ( currentBlock == dirFatId)
    {
        disk.read(currentBlock, (uint8_t*)this->workingDirectory);
    }
    return 0;
}

// mv <sourcepath> <destpath> renames the file <sourcepath> to the name <destpath>,
// or moves the file <sourcepath> to the directory <destpath> (if dest is a directory)
int
FS::mv(std::string sourcepath, std::string destpath)
{
    dir_entry dir[64];
    dir_entry destDir[64];
    int dirFatId, destFatId;
    if(this->getDirectory(sourcepath, dir, dirFatId, false) == -1)
    {
        std::cout << "ERROR: No dir found\n";
        return 0;
    }
    std::string file = getFile(sourcepath);

    if(this->getDirectory(destpath, destDir, destFatId, false) == -1)
    {
        std::cout << "ERROR: No dir found\n";
        return 0;
    }
    std::string destFile = getFile(destpath);

    //Making sure the first file exists
    int index = -1;
    for (int i = 0; i < numbEnteries(dir); i++)
    {
        if (dir[i].file_name == file)
        {
            if (dir[i].type == TYPE_DIR)
            {
                std::cout << "ERROR: The first needs to be a file\n";
                return 0;
            }
            
            index = i;
            break;
        }
    }

    if (index == -1)
    {
        std::cout << "ERROR: Could not find file\n";
        return 0;
    }

    //Seeing if the destination is a file or directory
    bool directory = false;
    for (int i = 0; i < numbEnteries(destDir); i++)
    {
        if (destDir[i].file_name == destFile)
        {
            if (destDir[i].type == TYPE_FILE)
            {
                std::cout << "ERROR: File already exists\n";
                return 0;
            }
            else if (destDir[i].type == TYPE_DIR)
            {
                this->getDirectory(destpath, destDir, destFatId, true);
                directory = true;
                break;
            }   
        }
    }

    if (directory)
    {
        for (int i = 0; i < numbEnteries(destDir); i++)
        {
            if (destDir[i].file_name == file)
            {
                std::cout << "ERROR: File already exists\n";
                return 0;
            }
        }
    }

    //Reads in the text if you have access to the file
    std::string fileText;
    int accessRight = dir[index].access_rights;
    if (accessRight == READ || accessRight == 0x06 || accessRight == 0x07)
    {
        readFromDisk(fileText, index, dir);
    }
    else
    {
        std::cout << "ERROR: Access denied\n";
        return 0;
    }

    //Creating the new file for workingdirectory
    int newIndex = numbEnteries(destDir);
    int block = -1;

    if (directory || destFile == "/")
    {
        //Adds file to new directory
        strcpy(destDir[newIndex].file_name, file.c_str());
        destDir[newIndex].size = dir[index].size;
        destDir[newIndex].type = dir[index].type;
        destDir[newIndex].access_rights = dir[index].access_rights;
        writeToDisk(fileText, destDir[newIndex].size, block, true);
        destDir[newIndex].first_blk = block;
        disk.write(destFatId, (uint8_t*)destDir);

        //Removes file from old directory
        int nrEntries = numbEnteries(dir);
        int fatIndex, lastPlace = dir[nrEntries-1].first_blk;
        dir[index] = dir[nrEntries-1];
        dir[nrEntries-1].type = TYPE_EMPTY;

        while (lastPlace != FAT_EOF)
        {
            fatIndex = fat[lastPlace];
            fat[lastPlace] = FAT_FREE;
            lastPlace = fatIndex;
        }
        disk.write(dirFatId, (uint8_t*)dir);
    }
    else
    {
        strcpy(dir[index].file_name, destFile.c_str());
        disk.write(dirFatId, (uint8_t*)dir);
    }

    disk.write(FAT_BLOCK, (uint8_t*)fat);
    if ( currentBlock == dirFatId)
    {
        disk.read(currentBlock, (uint8_t*)this->workingDirectory);
    }
    return 0;
}

// rm <filepath> removes / deletes the file <filepath>
int
FS::rm(std::string filepath)
{
    dir_entry dir[64];
    int dirFatId;
    if(this->getDirectory(filepath, dir, dirFatId, false) == -1)
    {
        std::cout << "ERROR: No dir found\n";
        return 0;
    }
    std::string file = getFile(filepath);

    int index = -1;
    bool directory = false;
    for (int i = 0; i < numbEnteries(dir); i++)
    {
        if (dir[i].file_name == file)
        {
            if (dir[i].type == TYPE_DIR)
            {
                getDirectory(filepath, dir, dirFatId, true);
                directory = true;
            }

            index = i;
            break;
        }
    }

    if (index == -1)
    {
        std::cout << "ERROR: Could not find file\n";
        return 0;
    }

    if (directory)
    {
        if (numbEnteries(dir) > 1)
        {
            std::cout << "ERROR: Directory is not empty\n";
            return 0;
        }
        else
        {
            //Removes directory
            getDirectory(filepath, dir, dirFatId, false);
            int nrEntries = numbEnteries(dir);
            dir[index] = dir[nrEntries-1];
            dir[nrEntries-1].type = TYPE_EMPTY;
            int fatIndex, lastPlace = dir[nrEntries-1].first_blk;

            while (lastPlace != FAT_EOF)
            {
                fatIndex = fat[lastPlace];
                fat[lastPlace] = FAT_FREE;
                lastPlace = fatIndex;
            }
            disk.write(dirFatId, (uint8_t*)dir);
        }
    }
    else
    {
        //Replaces and removes
        int nrEntries = numbEnteries(dir);
        int fatIndex, lastPlace = dir[nrEntries-1].first_blk;
        dir[index] = dir[nrEntries-1];
        dir[nrEntries-1].type = TYPE_EMPTY;

        while (lastPlace != FAT_EOF)
        {
            fatIndex = fat[lastPlace];
            fat[lastPlace] = FAT_FREE;
            lastPlace = fatIndex;
        }
        disk.write(dirFatId, (uint8_t*)dir);
    }

    disk.write(FAT_BLOCK, (uint8_t*)fat);
    if ( currentBlock == dirFatId)
    {
        disk.read(currentBlock, (uint8_t*)this->workingDirectory);
    }
    return 0;
}

// append <filepath1> <filepath2> appends the contents of file <filepath1> to
// the end of file <filepath2>. The file <filepath1> is unchanged.
int
FS::append(std::string filepath1, std::string filepath2)
{
    dir_entry dir[64];
    dir_entry destDir[64];
    int dirFatId;
    if(this->getDirectory(filepath1, dir, dirFatId, false) == -1)
    {
        std::cout << "ERROR: No dir found\n";
        return 0;
    }
    std::string file = getFile(filepath1);

    if(this->getDirectory(filepath2, destDir, dirFatId, false) == -1)
    {
        std::cout << "ERROR: No dir found\n";
        return 0;
    }
    std::string destFile = getFile(filepath2);

    //Making sure the first file exists
    int index1 = -1, index2 = -1;
    bool right1 = false, right2 = false;
    uint8_t accessRight;

    for (int i = 0; i < numbEnteries(dir); i++)
    {
        if (dir[i].file_name == file)
        {
            if (dir[i].type == TYPE_DIR)
            {
                std::cout << "ERROR: The first needs to be a file\n";
                return 0;
            }

            index1 = i;
            accessRight = dir[i].access_rights;
            if (accessRight == READ || accessRight == 0x06 || accessRight == 0x07)
            {
                right1 = true;
            }
            break;
        }
    }

    for (int i = 0; i < numbEnteries(destDir); i++)
    {
        if (destDir[i].file_name == destFile)
        {
            if (destDir[i].type == TYPE_DIR)
            {
                std::cout << "ERROR: The second needs to be a file\n";
                return 0;
            }
            
            index2 = i;
            accessRight = destDir[i].access_rights;
            if (accessRight == WRITE || accessRight == 0x06 || accessRight == 0x07)
            {
                right2 = true;
            }
            break;
        }
    }

    if (index1 == -1 || index2 == -1)
    {
        std::cout << "ERROR: File does not exist\n";
        return 0;
    }
    else if (!right1 || !right2)
    {
        std::cout << "ERROR: Access denied\n";
        return 0;
    }

    //Read and write to files
    std::string fileText;
    readFromDisk(fileText, index2, destDir);
    readFromDisk(fileText, index1, dir);

    int count = 0, temp = 0;
    std::string fixedText;
    int fileSize = fileText.size();
    int lastBlock = destDir[index2].first_blk;
    destDir[index2].size = fileSize;

    while (lastBlock != FAT_EOF)
    {
        fixedText = fileText.substr(4096 * count, 4096);
        disk.write(lastBlock, (uint8_t*)fixedText.c_str());
        lastBlock = fat[lastBlock];
        fileSize -= 4096;
        count++;
    }

    //If the file needs more blocks
    if (fileSize > 0)
    {
        writeToDisk(fileText, fileSize, temp, false);
    }

    disk.write(dirFatId, (uint8_t*)destDir);
    disk.write(FAT_BLOCK, (uint8_t*)fat);

    if (currentBlock == dirFatId)
    {
        disk.read(currentBlock, (uint8_t*)this->workingDirectory);
    }
    return 0;
}

// mkdir <dirpath> creates a new sub-directory with the name <dirpath>
// in the current directory
int
FS::mkdir(std::string dirpath)
{
    if(this->numbEnteries(this->workingDirectory) > 63)
    {
        std::cout << "ERROR: dir is full\n";
        return 0;
    }
    std::string name = this->getFile(dirpath);
    dir_entry dir[64];
    int dirFatId;
    bool fromRoot = dirpath[0] == '/';
    if(this->getDirectory(dirpath, dir, dirFatId, false) == -1)
    {
        std::cout << "ERROR: No dir found\n";
        return 0;
    }

    int num = this->numbEnteries(dir);
    if(num > 64)
    {
        std::cout << "ERROR: Directory full\n";
        return 0;
    }
    for (int i = 0; i < num; i++)
    {
        if(dir[i].file_name == name)
        {
            std::cout << "ERROR: Name exists\n";
            return 0;
        }
    }
    
    dir[num].type = TYPE_DIR;
    dir[num].access_rights = READWRITE;
    strcpy(dir[num].file_name, name.c_str());

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
    dir[num].first_blk = freeFat;
    folder[0].type = TYPE_DIR;
    folder[0].first_blk = dirFatId;
    std::string nname = "..";
    strcpy(folder[0].file_name, nname.c_str());

    this->writeDirToDisk(freeFat, folder);
    disk.write(FAT_BLOCK, (uint8_t*)fat);
    this->writeDirToDisk(dirFatId, dir);
    dir_entry test[64];
    int np;
    this->readDirBlock(freeFat, test, np);
    if(currentBlock == dirFatId)
    {
        disk.read(currentBlock, (uint8_t*)this->workingDirectory);
    }
    return 0;
}

// cd <dirpath> changes the current (working) directory to the directory named <dirpath>
int
FS::cd(std::string dirpath)
{
    dir_entry dir[64];
    if(this->getDirectory(dirpath, dir, this->currentBlock, true) == -1)
    {
        std::cout << "ERROR: no directory found\n";
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
    bool inRoot = false;
    int lastDirFatId, newDirFatId = this->currentBlock;
    std::vector<std::string> path;
    dir_entry dir[64];
    for (int i = 0; i < 64; i++)
    {
        dir[i] = this->workingDirectory[i];
    }
    std::string look = "..";
    while(!inRoot)
    {
        if(this->getDirectory(look, dir, lastDirFatId, true) == -1)
        {
            inRoot = true;
        } 
        else
        {
            for (int i = 0; i < 64; i++)
            {
                if(dir[i].first_blk == (uint16_t)newDirFatId)
                {
                    path.push_back(dir[i].file_name);
                    newDirFatId = lastDirFatId;
                    look.append("/..");
                    i = 100;
                }
            }
        }
    }

    for (int i = path.size(); i > 0;)
    {
        i--;
        std::cout << "/" << path[i];
    }
    if(path.size() > 0)
    {
        std::cout << "\n";
    }
    else
    {
        std::cout << "/\n";
    }

    return 0;
}

// chmod <accessrights> <filepath> changes the access rights for the
// file <filepath> to <accessrights>.
int
FS::chmod(std::string accessrights, std::string filepath)
{
    std::string name = this->getFile(filepath);
    dir_entry dir[64];
    int dirFatId;
    if(this->getDirectory(filepath, dir, dirFatId, false) == -1)
    {
        std::cout << "ERROR: No dir found\n";
        return 0;
    }
    for (int i = 0; i < 64; i++)
    {
        if(strcmp(dir[i].file_name, name.c_str()) == 0)
        {
            dir[i].access_rights = stoi(accessrights);
            disk.write(dirFatId, (uint8_t*)dir);
            if(currentBlock == dirFatId)
            {
                disk.read(currentBlock, (uint8_t*)this->workingDirectory);
            }
            return 0;
        }
    }
    std::cout << "ERROR: No file found\n";
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
void FS::readFromDisk(std::string& fileText, int fileIndex, dir_entry* dir)
{
    char* buffer;
    int lastPlace = dir[fileIndex].first_blk;

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
    bool fromRoot = false;

    for (int i = 0; i < path.size(); i++)
    {
        if (i == 0 && text[i] == '/')
        {
            fromRoot = true;
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
        if(fromRoot && directory.size() == 0) {}
        else
        {
            directories.push_back(directory);
        }
    }

    //dir_entry dirs[64];
    if(fromRoot)
    {
        disk.read(ROOT_BLOCK, (uint8_t*)dir);
        newBlock = 0;
    }
    else
    {
        for (int i = 0; i < 64; i++)
        {
            dir[i] = workingDirectory[i];
            newBlock = this->currentBlock;
        }
    }
    
    //If the file is in the same directory
    if (directories.size() == 0)
    {
        return 1;
    }

    bool found = false;
    int count = this->numbEnteries(this->workingDirectory);
    for (int i = 0; i < directories.size(); i++)
    {
        found = false;
        for (int j = 0; j < count; j++)
        {
            if(strcmp(dir[j].file_name, directories[i].c_str()) == 0 && dir[j].type == TYPE_DIR)
            {
                newBlock = dir[j].first_blk;
                disk.read(dir[j].first_blk, (uint8_t*)dir);                
                found = true;
                count = 64;
                break;
            }
        }
        if (!found)
        {
            return-1;
        }
    }
    return 1;
}