#include <string.h>
#include <vector>
#include <sys/stat.h>
#include "types.h"

#ifdef _MSC_VER
#include <io.h>
#else
#include <unistd.h>
#endif

#include "hw/mem/_vmem.h"

string user_data_dir;
std::vector<string> system_config_dirs;
std::vector<string> system_data_dirs;

void set_user_data_dir(const string& dir)
{
	user_data_dir = dir;
}

void add_system_config_dir(const string& dir)
{
	system_config_dirs.push_back(dir);
}

void add_system_data_dir(const string& dir)
{
	system_data_dirs.push_back(dir);
}

string get_writable_data_path(const string& filename)
{
   extern char game_dir_no_slash[1024];
   return std::string(game_dir_no_slash + 
#ifdef _WIN32
         std::string("\\")
#else
         std::string("/")
#endif
         + filename);
}

string get_writable_vmu_path(const char *logical_port)
{
   extern char vmu_dir_no_slash[PATH_MAX];
   extern char content_name[PATH_MAX];
   extern unsigned per_content_vmus;
   wchar filename[512];

   if ((per_content_vmus == 1 && !strcmp("A1", logical_port)) ||
       (per_content_vmus == 2))
   {
      sprintf(filename, "%s.%s.bin", content_name, logical_port);
      return std::string(vmu_dir_no_slash +
#ifdef _WIN32
         std::string("\\")
#else
         std::string("/")
#endif
         + filename);
   }
   else
   {
      sprintf(filename, "vmu_save_%s.bin", logical_port);
      return get_writable_data_path(filename);
   }
}
