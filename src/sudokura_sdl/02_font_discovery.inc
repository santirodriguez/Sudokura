static bool try_candidates(char out[PATH_MAX], const char* name){
  const char* dirs[16]; int n=0;
#if defined(_WIN32)
  dirs[n++]="C:\\Windows\\Fonts";
  const char* local=getenv("LOCALAPPDATA");
  if(local){static char buf[PATH_MAX];if(path_copy(buf,sizeof(buf),local)&&path_append(buf,sizeof(buf),"Microsoft\\Windows\\Fonts",'\\'))dirs[n++]=buf;}
#else
  dirs[n++]="/usr/share/fonts/truetype/dejavu";
  dirs[n++]="/usr/share/fonts/truetype/noto";
  dirs[n++]="/usr/share/fonts/truetype/liberation2";
  dirs[n++]="/usr/share/fonts";
  dirs[n++]="/usr/local/share/fonts";
  const char* home=getenv("HOME");
  static char buf1[PATH_MAX],buf2[PATH_MAX];
  if(home){
    if(path_copy(buf1,sizeof(buf1),home)&&path_append(buf1,sizeof(buf1),".local/share/fonts",'/'))dirs[n++]=buf1;
    if(path_join(buf2,sizeof(buf2),home,".fonts",'/'))dirs[n++]=buf2;
  }
# if defined(__APPLE__)
  dirs[n++]="/System/Library/Fonts";
  dirs[n++]="/Library/Fonts";
  if(home){static char buf3[PATH_MAX];if(path_join(buf3,sizeof(buf3),home,"Library/Fonts",'/'))dirs[n++]=buf3;}
# endif
#endif
  for(int i=0;i<n;i++){
    char p[PATH_MAX];
#if defined(_WIN32)
    if(!path_join(p,sizeof(p),dirs[i],name,'\\'))continue;
#else
    if(!path_join(p,sizeof(p),dirs[i],name,'/'))continue;
#endif
    if(try_open_font_path(p)&&path_copy(out,PATH_MAX,p))return true;
  }
  return false;
}

static bool get_exe_dir(char out[PATH_MAX]){
  char*base=SDL_GetBasePath();
  if(base&&base[0]){
    bool ok=path_copy(out,PATH_MAX,base);
    SDL_free(base);
    if(ok)return true;
  }else if(base){
    SDL_free(base);
  }
#if defined(_WIN32)
  char buf[PATH_MAX];DWORD n=GetModuleFileNameA(NULL,buf,PATH_MAX);
  if(n==0||n>=PATH_MAX)return false;
  for(size_t i=0;i<n;i++)if(buf[i]=='/')buf[i]='\\';
  char*slash=strrchr(buf,'\\');if(!slash)return false;*slash='\0';
  return path_copy(out,PATH_MAX,buf);
#elif defined(__APPLE__)
  char buf[PATH_MAX];uint32_t size=(uint32_t)sizeof(buf);
  if(_NSGetExecutablePath(buf,&size)!=0)return false;
  char*slash=strrchr(buf,'/');if(!slash)return false;*slash='\0';
  return path_copy(out,PATH_MAX,buf);
#else
  char buf[PATH_MAX];ssize_t n=readlink("/proc/self/exe",buf,sizeof(buf)-1);
  if(n<=0)return false;buf[n]='\0';
  char*slash=strrchr(buf,'/');if(!slash)return false;*slash='\0';
  return path_copy(out,PATH_MAX,buf);
#endif
}

static bool try_in_dir(const char*dir,const char*name,char out[PATH_MAX]){
  if(!dir||!dir[0]||!name)return false;
  char p[PATH_MAX];
#if defined(_WIN32)
  if(!path_join(p,sizeof(p),dir,name,'\\'))return false;
#else
  if(!path_join(p,sizeof(p),dir,name,'/'))return false;
#endif
  return try_open_font_path(p)&&path_copy(out,PATH_MAX,p);
}

static const char*find_font_path_dynamic(char out[PATH_MAX],const char*cli){
  /* Explicit developer/user override always wins. */
  if(cli&&try_open_font_path(cli)&&path_copy(out,PATH_MAX,cli))return out;

  /* Packaged resource first: portable ZIP/AppImage place DejaVu beside the
     executable; macOS additionally places it in Contents/Resources. */
  char exedir[PATH_MAX]={0};
  bool have_exe_dir=get_exe_dir(exedir);
  const char*packaged[]={"DejaVuSans.ttf","NotoSans-Regular.ttf"};
  for(size_t i=0;i<sizeof(packaged)/sizeof(packaged[0]);++i){
    if(have_exe_dir&&try_in_dir(exedir,packaged[i],out))return out;
#if defined(__APPLE__)
    if(have_exe_dir){
      char resources[PATH_MAX]={0};
      if(path_join(resources,sizeof(resources),exedir,"../Resources",'/')&&
         try_in_dir(resources,packaged[i],out))return out;
    }
#endif
  }

  /* Explicit development paths after package paths, never recursive. */
  const char*dev[]={"DejaVuSans.ttf","NotoSans-Regular.ttf",
                    "DejaVuSans-Regular.ttf"};
  for(size_t i=0;i<sizeof(dev)/sizeof(dev[0]);++i)
    if(try_open_font_path(dev[i])&&path_copy(out,PATH_MAX,dev[i]))return out;

  /* Bounded platform fallback list. */
  const char*preferred[]={
    "DejaVuSans.ttf","DejaVuSans-Regular.ttf","NotoSans-Regular.ttf",
    "LiberationSans-Regular.ttf","FreeSans.ttf","Arial.ttf","Ubuntu-R.ttf",
    "Cantarell-VF.otf","SFNS.ttf"
  };
  for(size_t i=0;i<sizeof(preferred)/sizeof(preferred[0]);++i)
    if(try_candidates(out,preferred[i]))return out;
  return NULL;
}
