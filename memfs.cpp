#define FUSE_USE_VERSION 26
#include <errno.h>
#include <fuse.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/statvfs.h>

#include <map>
#include <iostream>
#include <string>
using namespace std;
#define BLOCKSIZE 512UL
#define MAX_NAME   255
#define MAX_INODE    (512UL * 1024)
#define MAX_BLOCKS   (512UL * 1024)

#define M_ATIME (1 << 0)
#define M_CTIME (1 << 1)
#define M_MTIME (1 << 2)
#define M_ALL   (M_ATIME | M_CTIME | M_MTIME)

class memfs{
public:
    struct inode{ 
        string path;
        struct stat vstat;
        char* data;
    };
    static inode* root;
    static struct statvfs m_statvfs;
    static map<string,inode*> g_dirMap;
    static int statfs(const char *path, struct statvfs *stbuf);
    static int getattr(const char *path, struct stat *stbuf);
    static int mkdir(const char *path, mode_t mode);
    static int open(const char *path, struct fuse_file_info *fi);
    static int mknod(const char *path, mode_t mode, dev_t rdev);
    static int write(const char *path,
                    const char *buf, size_t size, off_t offset,
                    struct fuse_file_info *fi);
    static int read(const char *path,
                   char *buf, size_t size, off_t offset,
                   struct fuse_file_info *fi);
    static int readdir(const char *path, void *buf, fuse_fill_dir_t filler,
                             off_t offset, struct fuse_file_info *fi);
    static int __readdir__(const char *dirname, void *buf, fuse_fill_dir_t filler);
    static inline void __update_times__(inode *pf, int mask);
    static void __init__();
    static struct fuse_operations memfs_oper; 
};

struct fuse_operations memfs::memfs_oper;
memfs::inode* memfs::root = new inode{"/",{},nullptr};
struct statvfs memfs::m_statvfs = {BLOCKSIZE, BLOCKSIZE, MAX_BLOCKS, MAX_BLOCKS, MAX_BLOCKS,
                                   MAX_INODE, MAX_INODE, MAX_INODE,  0x9876543210, ST_NOSUID, MAX_NAME};
map<string,memfs::inode*> memfs::g_dirMap{make_pair("/",memfs::root)};

void memfs::__init__(){
    //init root inode
    (root->vstat).st_mode = S_IFDIR | 0755;
    (root->vstat).st_nlink = 1;
    //init memfs_oper
    memfs_oper.getattr = getattr;
    memfs_oper.open    = open;
    memfs_oper.read    = read;
    memfs_oper.write   = write;
    memfs_oper.mknod   = mknod;
    memfs_oper.readdir = readdir;
    memfs_oper.mkdir   = mkdir;
    memfs_oper.statfs  = statfs;
}
/** Helper func, update time
 */
inline void memfs::__update_times__(inode *pf, int mask)
{
    time_t now = time(0);
    if (mask & M_ATIME) {
	pf->vstat.st_atime = now;
    }
    if (mask & M_CTIME) {
	pf->vstat.st_ctime = now;
    }
    if (mask & M_MTIME) {
	pf->vstat.st_mtime = now;
    }
}

/** Get file system statistics
 */
int memfs::statfs(const char *path, struct statvfs *stbuf)
{
    *stbuf = m_statvfs;
    return 0;
}

/** Get file attributes.
 */
int memfs::getattr(const char *path, struct stat *stbuf)
{
    int res = 0;
    memset(stbuf, 0, sizeof(struct stat));
    
    inode* pf;
    auto iter = g_dirMap.find(path);
    if(iter!=g_dirMap.end()){
        pf = iter->second;
        *stbuf = pf->vstat;
    }
    else res = -ENOENT;  
    return res;
}
 
/* Create a directory */ 
int memfs::mkdir(const char *path, mode_t mode)
{
    int res = 0;
    inode *pf = NULL;
    printf("%s: %s\n", __FUNCTION__, path); 
 
    pf = new inode{path, {}, nullptr};
    
    if (!pf) {
        return -ENOMEM;      
    }else{
        (pf->vstat).st_mode = S_IFDIR | mode;
    }
 
    auto ret = g_dirMap.insert(make_pair(path,pf)); 
    if (ret.second == false) {
        delete pf; 
        res = -EEXIST;       
    }///> File exist!
    __update_times__(pf, M_ALL);
    return res;
}

int memfs::open(const char *path, struct fuse_file_info *fi)
{
    int res = 0;
    inode *pf = NULL;
    printf("%s: %s\n", __FUNCTION__, path);
 
    auto iter = g_dirMap.find(path);
    if(iter!=g_dirMap.end()){
        pf = iter->second;
        if (S_ISDIR(pf->vstat.st_mode)) {
            return -EISDIR;
        }
    }
    else{
        if ((fi->flags & O_ACCMODE) == O_RDONLY ||
            !(fi->flags & O_CREAT)) {
            return -ENOENT;
        }
        pf = new inode{path, {}, nullptr}; 
        if(!pf){
            (pf->vstat).st_mode = S_IFREG | 0755;
            g_dirMap.insert(make_pair(path,pf));
        }else{
            return -ENOMEM;
        }
    }
 
    fi->fh = (unsigned long)pf; 
    return res;
}

int memfs::mknod(const char *path, mode_t mode, dev_t rdev)
{
    int res = 0;
    inode *pf = NULL;
    printf("%s: %s\n", __FUNCTION__, path);
 
    pf = new inode{path, {}, nullptr}; 
    if (!pf) {
        return -ENOMEM;
    }else{
        (pf->vstat).st_mode = S_IFREG | 0755;
    }
 
    auto ret = g_dirMap.insert(make_pair(path,pf));
    if (ret.second == false) {
        delete pf;
        res = -EEXIST;
    }///> File exist!
 
    m_statvfs.f_favail = --m_statvfs.f_ffree;
 
    return res;
}

/** Write data to an open file*/ 
int memfs::write(const char *path,
                 const char *buf, size_t size, off_t offset,
                 struct fuse_file_info *fi)
{
    inode *pf = (inode *)fi->fh;
    fsblkcnt_t total_blocks = (offset + size + BLOCKSIZE - 1) / BLOCKSIZE;
    fsblkcnt_t req_blocks = total_blocks - pf->vstat.st_blocks;
    if (m_statvfs.f_bavail < req_blocks)
        return -ENOMEM;
    if (pf->vstat.st_blocks < (blkcnt_t)total_blocks) {
        char *ndata = (char *)realloc(pf->data, total_blocks * BLOCKSIZE);
        if (!ndata) { 
            return -ENOMEM;
        }   
        m_statvfs.f_bfree = m_statvfs.f_bavail -= req_blocks;
        pf->data = ndata;
        pf->vstat.st_blocks = total_blocks;
    }   
    memcpy(pf->data + offset, buf, size);
    
    // Update file size if necessary
    off_t nsize = offset + size;
    if (nsize > pf->vstat.st_size) {
        pf->vstat.st_size = nsize; 
    }   
    
    __update_times__(pf, M_ALL);
    return size;
}   
 
/** Read data from an open file*/
int memfs::read(const char *path,
                char *buf, size_t size, off_t offset,
                struct fuse_file_info *fi)
{
    inode *pf = (inode *)fi->fh; 
    printf("%s: %s\n", __FUNCTION__, path); 
 
    off_t filesize = pf->vstat.st_size; 
    if (offset > filesize) {
        return 0;
    }
 
    size_t avail = filesize - offset; 
    size_t rsize = (size < avail) ? size : avail;
    memcpy(buf, pf->data + offset, rsize);
 
    __update_times__(pf, M_ATIME);
    return rsize;
}

inline const char *__is_parent(const char *parent, const char *path)
{
    const char delim = '/';
 
    if (parent[1] == '\0' && parent[0] == '/' && path[0] == '/') {
        return path;
    }
 
    while (*parent != '\0' && *path != '\0' && *parent == *path) {
        ++parent, ++path;
    }
    return (*parent == '\0' && *path == delim) ? path : NULL;
}
 
int memfs::__readdir__(const char *dirname, void *buf, fuse_fill_dir_t filler)
{
    auto iter = g_dirMap.find(dirname);
    inode* pf;
    if(iter!=g_dirMap.end()){
        pf = iter->second;
        if (!S_ISDIR(pf->vstat.st_mode)) {
            return -ENOTDIR;
        }
    }
    else 
        return -ENOENT;  
 
    for(iter++;iter!=g_dirMap.end();iter++){
        const inode *pf = iter->second;
        const char *basename = __is_parent(dirname, (pf->path).c_str());
 
        if (!basename) {
            break;
        }
        else if (strchr(basename + 1, '/')) {
            continue;
        }
        filler(buf, basename + 1, &pf->vstat, 0);
        printf(" readdir: %10s, path: %10s\n", basename, pf->path.c_str());
    }
 
    return 0;
}
 
int memfs::readdir(const char *path, void *buf, fuse_fill_dir_t filler,
                   off_t offset, struct fuse_file_info *fi)
{
    int res = 0;
    printf("%s: %s\n", __FUNCTION__, path);
 
    filler(buf, ".", NULL, 0);
 
    if (strcmp(path, "/") != 0) {
        filler(buf, "..", NULL, 0);
    }
 
    res = __readdir__(path, buf, filler);
    return res;
}

int main(int argc, char** argv) {
    std::cout << "Hello from " << argv[1] << ". I got " << argc << " arguments\n" << std::endl;
    fprintf(stderr, "libfuse majv. %d. minv. %d\n", FUSE_MAJOR_VERSION, FUSE_MINOR_VERSION);
    memfs::__init__();
    return fuse_main(argc, argv, &memfs::memfs_oper, NULL);
}
