//adding necessary libaries
#include "myfs.h"
#include <string.h>
#include <iostream>
#include <math.h>
#include <sstream>
#include <algorithm>

//define "magic" numbers
#define SIZE 1024
#define FILESIZE 1024 * 1024
#define INITIAL_SPACE 10
#define RESET 0
//define global variables
const char* MyFs::MYFS_MAGIC = "MYFS";
int curr_addr = 0;


MyFs::MyFs(BlockDeviceSimulator* blkdevsim_) :blkdevsim(blkdevsim_) {
	struct myfs_header header;
	blkdevsim->read(0, sizeof(header), (char*)&header);

	if (strncmp(header.magic, MYFS_MAGIC, sizeof(header.magic)) != 0 ||
		(header.version != CURR_VERSION)) {
		std::cout << "Did not find myfs instance on blkdev" << std::endl;
		std::cout << "Creating..." << std::endl;
		format();
		std::cout << "Finished!" << std::endl;
	}
}

void MyFs::format()
{
	// put the header in place
	struct myfs_header header;
	strncpy(header.magic, MYFS_MAGIC, sizeof(header.magic));
	header.version = CURR_VERSION;
	blkdevsim->write(0, sizeof(header), (const char*)&header);
	curr_addr += 5;//saving empty space
	const char* c = "$";
	for (int i = curr_addr; i < SIZE * SIZE; i++)//initiate all file data as $
	{
		blkdevsim->write(i, 1, c);
	}
}

void MyFs::create_file(std::string path_str, bool directory)
{
	char file[SIZE];
	std::string path;
	for (int i = RESET; i < SIZE; i++)
	{
		blkdevsim->read((SIZE * i) + INITIAL_SPACE, SIZE, file);
		for (int j = RESET; j < SIZE; j++)
		{
			if (file[j] == '$')
			{
				path = path_str + "{}";
				blkdevsim->write(j + INITIAL_SPACE, path.length(), path.c_str());
				break;
			}
		}
		break;
	}

}

std::string MyFs::get_content(std::string path_str)
{
	//declaring local vars
	char file[SIZE];
	int loc = RESET;
	std::string data = "";

	for (int i = RESET; i < SIZE; i++)//loop to find first occurrence of "{" then pull data until his matching bracket
	{
		blkdevsim->read((SIZE * i) + INITIAL_SPACE, SIZE, file);
		std::string str(file);
		size_t first_occurr = str.find(path_str + "{");
		if (std::string::npos != first_occurr)
		{
			data = str.substr(first_occurr);
			data = data.substr(0, '}');
		}
	}
	char file2[FILESIZE + INITIAL_SPACE];
	blkdevsim->read(INITIAL_SPACE, FILESIZE - INITIAL_SPACE, file2);//get data from block device
	std::string name;
	int size = 0, j = 0, k = 0;
	for (int i = 0; i < FILESIZE - INITIAL_SPACE && file2[i] != '$'; i++)//ran all over data to find what we were looking for
	{
		for (j = i; j < FILESIZE - INITIAL_SPACE && file2[j] != '{' && file2[j] != '$'; j++)//get name of file until his data start by context file_name{data}
			name += file2[j];
		if (name == path_str)//if we found the name we were looking for we will find the lenght of his data now
		{
			for (k = j + 1; k < FILESIZE - INITIAL_SPACE && file2[k] != '}' && file2[k] != '$'; k++)
				size++;
			break;
		}
		name = "";
		i = j;
	}
	data.erase(std::remove(data.begin(), data.end(), '$'), data.end());//remove all unecessary things from string data	->	$
	data.erase(std::remove(data.begin(), data.end(), '\n'), data.end());//remove all unecessary things from string data	->	\n
	data.erase(std::remove(data.begin(), data.end(), '{'), data.end());//remove all unecessary things from string data	->	{
	data.erase(std::remove(data.begin(), data.end(), '}'), data.end());//remove all unecessary things from string data	-> }
	data = data.substr(data.find(path_str) + path_str.size());//remove all unecessary things from string data	->	name of file
	data = data.substr(0, size - 1);//remove all unecessary things from string data	->	rest of data
	return data;
}

void MyFs::set_content(std::string path_str, std::string content)
{
	//declaring local vars
	char file[SIZE];
	int loc = RESET;
	int slice = RESET;
	int fend = RESET;
	std::string data = "";

	for (int i = RESET; i < SIZE; i++)//loop to find first occurrence of "{" then pull data until his matching bracket
	{
		blkdevsim->read((SIZE * i) + INITIAL_SPACE, SIZE, file);
		std::string str(file);
		size_t first_occurr = str.find(path_str + "{");
		if (std::string::npos != first_occurr)
		{
			loc = INITIAL_SPACE + first_occurr + (1024 * i);
			break;
		}
	}
	if (loc != RESET)//if we found we will look for the second one now
	{
		char end[FILESIZE - loc];
		blkdevsim->read(loc, FILESIZE - loc, end);
		for (int i = 0; i < FILESIZE - loc; i++)
		{
			if (end[i] == '}')
			{
				fend = loc + i;
				break;//found it !!! :)
			}
		}
		char duplicate[FILESIZE - fend];
		blkdevsim->read(fend, FILESIZE - fend, duplicate);
		for (int i = 0; i < FILESIZE - fend; i++)//because we don't wont any gaps of memory we will take all the rest of the file closer or further in case content lenght isn't the same as before
		{
			if (duplicate[i] == '$')
			{
				slice = fend + i;
				break;
			}
		}
		char cut[slice];
		blkdevsim->read(fend, slice, cut);
		std::string copy_content = content;
		content[content.length()] = NULL;
		copy_content += "}";
		blkdevsim->write(path_str.length() + loc + 1, content.length(), copy_content.c_str());//change the content
		blkdevsim->write(loc + path_str.length() + 1 + content.length(), slice, cut);//add all the rest of the file closer or further in case content lenght isn't the same as before
	}
}

MyFs::dir_list MyFs::list_dir(std::string path_str)
{
	dir_list ans;
	char file[FILESIZE + INITIAL_SPACE];
	blkdevsim->read(INITIAL_SPACE, FILESIZE - INITIAL_SPACE, file);
	std::string name;
	int size = 0, j = 0, k = 0;
	for (int i = 0; i < FILESIZE - INITIAL_SPACE && file[i] != '$'; i++)//ran all over file
	{
		for (j = i; j < FILESIZE - INITIAL_SPACE && file[j] != '{' && file[j] != '$'; j++)//get name of file until his data start by context file_name{data}
			name += file[j];
		for (k = j + 1; k < FILESIZE - INITIAL_SPACE && file[k] != '}' && file[k] != '$'; k++)//get size of data of the same file
			size++;
		dir_list_entry curr = { name, false, size };//create dir_list_entry with name and size
		name = "";
		size = 0;
		i = k;
		ans.push_back(curr);//push dir_list_entry to vector
	}
	return ans;//return vector
}

