static bool try_candidates(char out[PATH_MAX], const char* name){
  const char* dirs[16]; int n=0;
#if defined(_WIN32)
  dirs[n++]="C:\\\\Windows\\\\Fonts";
  const char* local= getenv("LOCALAPPDATA");
  if(local){ static char buf[PATH_MAX]; if(path_copy(buf,sizeof(buf),local)&&path_append(buf,sizeof(buf),"Microsoft\\Windows\\Fonts",'\\'))dirs[n++]=buf; }
#else
  dirs[n++]="/usr/share/fonts";
  dirs[n++]="/usr/local/share/fonts";
  const char* home=getenv("HOME");
  static char buf1[PATH_MAX], buf2[PATH_MAX];
  if(home){ if(path_copy(buf1,sizeof(buf1),home)&&path_append(buf1,sizeof(buf1),".local/share/fonts",'/'))dirs[n++]=buf1; if(path_join(buf2,sizeof(buf2),home,".fonts",'/'))dirs[n++]=buf2; }
# if defined(__APPLE__)
  dirs[n++]="/System/Library/Fonts";
  dirs[n++]="/Library/Fonts";
  if(home){ static char buf3[PATH_MAX]; if(path_join(buf3,sizeof(buf3),home,"Library/Fonts",'/'))dirs[n++]=buf3; }
# endif
#endif
  for(int i=0;i<n;i++){
    char p[PATH_MAX];
#if defined(_WIN32)
    if(!path_join(p,sizeof(p),dirs[i],name,'\\'))continue;
#endif
#if !defined(_WIN32)
    if(!path_join(p,sizeof(p),dirs[i],name,'/'))continue;
#endif
    if(try_open_font_path(p)&&path_copy(out,PATH_MAX,p))return true;
  }
  return false;
}

#if defined(_WIN32)
static bool search_dir_win(const char* dir, char out[PATH_MAX]){
  char pattern[PATH_MAX]; if(!path_join(pattern,sizeof(pattern),dir,"*.*",'\\'))return false;
  WIN32_FIND_DATAA fd; HANDLE h=FindFirstFileA(pattern,&fd); if(h==INVALID_HANDLE_VALUE) return false;
  do{
    if(strcmp(fd.cFileName,".")==0 || strcmp(fd.cFileName,"..")==0) continue;
    char path[PATH_MAX]; if(!path_join(path,sizeof(path),dir,fd.cFileName,'\\'))continue;
    if(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY){
      if(search_dir_win(path,out)){ FindClose(h); return true; }
    }else{
      if(ends_withi(path,".ttf") || ends_withi(path,".otf")){
        if(try_open_font_path(path)&&path_copy(out,PATH_MAX,path)){ FindClose(h); return true; }
      }
    }
  }while(FindNextFileA(h,&fd));
  FindClose(h); return false;
}
#else
static bool search_dir_posix(const char* dir, char out[PATH_MAX]){
  DIR* d=opendir(dir); if(!d) return false;
  struct dirent* ent;
  while((ent=readdir(d))){
    if(ent->d_name[0]=='.') continue;
    char path[PATH_MAX]; if(!path_join(path,sizeof(path),dir,ent->d_name,'/'))continue;
    struct stat st; if(stat(path,&st)!=0) continue;
    if(S_ISDIR(st.st_mode)){
      if(search_dir_posix(path,out)){ closedir(d); return true; }
    }else{
      if(ends_withi(path,".ttf") || ends_withi(path,".otf")){
        if(try_open_font_path(path)&&path_copy(out,PATH_MAX,path)){ closedir(d); return true; }
      }
    }
  }
  closedir(d); return false;
}
#endif

/* === NUEVO: helpers para localizar el directorio del ejecutable y probar fuentes locales === */
static bool get_exe_dir(char out[PATH_MAX]) {
#if defined(_WIN32)
  char buf[PATH_MAX]; DWORD n = GetModuleFileNameA(NULL, buf, PATH_MAX);
  if(n==0 || n>=PATH_MAX) return false;
  for(size_t i=0;i<n;i++) if(buf[i]=='/') buf[i]='\\';
  char *slash = strrchr(buf, '\\');
  if(!slash) return false;
  *slash = '\0';
  return path_copy(out,PATH_MAX,buf);
#elif defined(__APPLE__)
  char buf[PATH_MAX]; uint32_t size = (uint32_t)sizeof(buf);
  if(_NSGetExecutablePath(buf, &size)!=0) return false;
  char *slash = strrchr(buf, '/');
  if(!slash) return false;
  *slash = '\0';
  return path_copy(out,PATH_MAX,buf);
#else
  char buf[PATH_MAX];
  ssize_t n = readlink("/proc/self/exe", buf, sizeof(buf)-1);
  if(n<=0) return false;
  buf[n] = 0;
  char *slash = strrchr(buf, '/');
  if(!slash) return false;
  *slash = '\0';
  return path_copy(out,PATH_MAX,buf);
#endif
}

static bool try_in_dir(const char*dir, const char*name, char out[PATH_MAX]){
  if(!dir||!name) return false;
  char p[PATH_MAX];
#if defined(_WIN32)
  if(!path_join(p,sizeof(p),dir,name,'\\'))return false;
#else
  if(!path_join(p,sizeof(p),dir,name,'/'))return false;
#endif
  if(try_open_font_path(p)&&path_copy(out,PATH_MAX,p))return true;
  return false;
}

/* Reforzado: primero cwd y directorio del ejecutable; luego rutas del sistema */
static const char* find_font_path_dynamic(char out[PATH_MAX], const char* cli){
  /* 0) CLI explícito */
  if(cli && try_open_font_path(cli)&&path_copy(out,PATH_MAX,cli))return out;

  /* 1) Local (cwd + exe dir) */
  const char* local_first[] = {
    "NotoSans-Regular.ttf",
    "DejaVuSans.ttf",
    "DejaVuSans-Regular.ttf",
    "Arial.ttf"
  };
  char exedir[PATH_MAX]={0};
  bool have_exe_dir = get_exe_dir(exedir);
#if defined(__APPLE__)
  char resources[PATH_MAX]={0};
  bool have_resources=have_exe_dir&&path_join(resources,sizeof(resources),exedir,"../Resources",'/');
#endif

  for(size_t i=0;i<sizeof(local_first)/sizeof(local_first[0]); ++i){
    /* cwd */
    if(try_open_font_path(local_first[i])&&path_copy(out,PATH_MAX,local_first[i]))return out;
    /* exe dir */
    if(have_exe_dir && try_in_dir(exedir, local_first[i], out)) return out;
#if defined(__APPLE__)
    /* App bundle resources live beside Contents/MacOS. */
    if(have_resources && try_in_dir(resources,local_first[i],out)) return out;
#endif
  }

  /* 2) Preferidas en ubicaciones conocidas del sistema */
  const char* preferred[]={
    "DejaVuSans.ttf","DejaVuSans-Regular.ttf","NotoSans-Regular.ttf","LiberationSans-Regular.ttf",
    "FreeSans.ttf","Arial.ttf","Ubuntu-R.ttf","Cantarell-VF.otf","SFNS.ttf"
  };
  for(size_t i=0;i<sizeof(preferred)/sizeof(preferred[0]);++i)
    if(try_candidates(out,preferred[i])) return out;

  /* 3) Escaneo profundo como último recurso */
#if defined(_WIN32)
  const char* roots[]={
    "C:\\\\Windows\\\\Fonts",
    getenv("LOCALAPPDATA")? getenv("LOCALAPPDATA") : ""
  };
  for(size_t i=0;i<sizeof(roots)/sizeof(roots[0]);++i){
    if(roots[i] && roots[i][0]){
      char start[PATH_MAX];
      bool valid=i==1?path_join(start,sizeof(start),roots[i],"Microsoft\\Windows\\Fonts",'\\'):path_copy(start,sizeof(start),roots[i]);
      if(!valid)continue;
      if(search_dir_win(start,out)) return out;
    }
  }
#else
  const char* roots[]={"/usr/share/fonts","/usr/local/share/fonts",
                       getenv("HOME")? getenv("HOME") : NULL,
#if defined(__APPLE__)
                       "/System/Library/Fonts","/Library/Fonts"
#endif
  };
  for(size_t i=0;i<sizeof(roots)/sizeof(roots[0]);++i){
    if(!roots[i]) continue;
    char start[PATH_MAX];
    if(roots[i]==getenv("HOME")){
      if(path_join(start,sizeof(start),roots[i],".local/share/fonts",'/')&&search_dir_posix(start,out)) return out;
      if(path_join(start,sizeof(start),roots[i],".fonts",'/')&&search_dir_posix(start,out)) return out;
#if defined(__APPLE__)
      if(path_join(start,sizeof(start),roots[i],"Library/Fonts",'/')&&search_dir_posix(start,out)) return out;
#endif
    }else{
      if(path_copy(start,sizeof(start),roots[i])&&search_dir_posix(start,out)) return out;
    }
  }
#endif
  return NULL;
}

