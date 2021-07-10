#include <dirent.h>
#include <math.h>
#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <sys/syscall.h>
#include <sys/uio.h>
#include <unistd.h>

typedef struct {
  float X, Y;
} Rect;
typedef struct {
  long id, hp, d, zhs, buff;
} Longaddr;
typedef struct {
  int id, zhscd, dcd, buffcd;
  float mapX, mapY, entX, enty, hp;
} Datavalue;

Longaddr StaticAddress[10];

Datavalue DynamicData[10];

pid_t target_pid = -1;
int hero, size_w, px, py, fx = 0;
long matrixaddr;

int findpid(const char *process_name) {
  int id;
  pid_t pid = -1;
  DIR *dir;
  FILE *fp;
  char filename[32];
  char cmdline[256];
  struct dirent *entry;
  if (process_name == NULL)
    return -1;
  dir = opendir("/proc");
  if (dir == NULL) {
    return -1;
  }
  while ((entry = readdir(dir)) != NULL) {
    id = atoi(entry->d_name);
    if (id != 0) {
      sprintf(filename, "/proc/%d/cmdline", id);
      fp = fopen(filename, "r");
      if (fp) {
        fgets(cmdline, sizeof(cmdline), fp);
        fclose(fp);
        if (strcmp(process_name, cmdline) == 0) {
          pid = id;
          break;
        }
      }
    }
  }
  closedir(dir);
  return pid;
}

long get_module_base(pid_t pid, char *module_name) {
  long startaddr = 0;
  char *pm, om[64], path[256], line[1024];
  bool bssOF = false, LastIsSo = false;
  strcpy(om, module_name);
  pm = strtok(om, ":");
  module_name = pm;
  pm = strtok(NULL, ":");
  if (pm) {
    if (strcmp(pm, "bss") == 0) {
      bssOF = true;
    }
  }
  sprintf(path, "/proc/%d/maps", pid);
  FILE *p = fopen(path, "r");
  if (p) {
    while (fgets(line, sizeof(line), p)) {
      if (LastIsSo) {
        if (strstr(line, "[anon:.bss]") != NULL) {
          sscanf(line, "%lx-%*lx", &startaddr);
          break;
        } else {
          LastIsSo = false;
        }
      }
      if (strstr(line, module_name) != NULL) {
        if (!bssOF) {
          sscanf(line, "%lx-%*lx", &startaddr);
          break;
        } else {
          LastIsSo = true;
        }
      }
    }
    fclose(p);
  }
  return startaddr;
}

void *findpid1(void *arg) {
  for (;;) {
    if ((int)findpid("com.tencent.tmgp.sgame") == -1) {
      exit(0);
    }
    sleep((int)1);
  }
}

void readBuffer(long addr, void *buffer, int size) {
  struct iovec iov_ReadBuffer, iov_ReadOffset;
  iov_ReadBuffer.iov_base = buffer;
  iov_ReadBuffer.iov_len = size;
  iov_ReadOffset.iov_base = (void *)addr;
  iov_ReadOffset.iov_len = size;
  syscall(SYS_process_vm_readv, target_pid, &iov_ReadBuffer, 1, &iov_ReadOffset,
          1, 0);
}

int readInt(long address) {
  int value = 0;
  int *p = &value;
  readBuffer(address, p, sizeof(int));
  return value;
}

float readFloat(long address) {
  float value = 0.0;
  float *p = &value;
  readBuffer(address, p, sizeof(float));
  return value;
}

long readValueL(long address) {
  long addr = 0;
  long *p = &addr;
  readBuffer(address, p, sizeof(long));
  return addr & 0xFFFFFFFFFF;
}

long ValueLmultiple(long address, long Offset[], char bytelength[]) {
  for (int i = 0; i < (int)atoi(bytelength); i++) {
    address = readValueL(address) + Offset[i];
  }
  return address;
}

void getscreen() {
  long resolutionratio =
      get_module_base(target_pid, (char *)"libunity.so:bss") + 0x19FF2C;
  size_w = readInt(resolutionratio + 0x4);
  px = readInt(resolutionratio) / 2;
  py = size_w / 2;
}

Rect matrixmem(int X1, int Y1, const float MatrixMem[]) {
  Rect rect;
  float Xm = X1 * 0.001;
  float Zm = Y1 * 0.001;
  rect.X = 0.0, rect.Y = 0.0;
  float Rai = fabs(Zm * MatrixMem[11] + MatrixMem[15]);
  if (Rai > 0.01) {
    rect.X = px + (Xm * MatrixMem[0] + MatrixMem[12]) * px / Rai;
    rect.Y = py - (Zm * MatrixMem[9] + MatrixMem[13]) * py / Rai;
  }
  return rect;
}

void data() {
  for (int i = 0; i < hero; i++) {
    DynamicData[i].id = readInt(StaticAddress[i].id + 0x20); // 英雄id
    StaticAddress[i].hp = readValueL(StaticAddress[i].id + 0xE8) + 0xF8; // 血量
    StaticAddress[i].d = ValueLmultiple(
        StaticAddress[i].id + 0xC8, new long[3]{(0x98 + 2 * 0x10), 0x78, 0x2C},
        (char *)"3"); // 大招cd
    StaticAddress[i].zhs = ValueLmultiple(
        StaticAddress[i].id + 0xC8, new long[3]{(0x98 + 5 * 0x10), 0x78, 0x2C},
        (char *)"3"); // 召唤师cd地址
  }
}

void buff(int pdid) {
  int order[2][4] = {{2, 3, 0, 1}, {0, 1, 2, 3}};
  long libGameCore_startaddr =
      get_module_base(target_pid, (char *)"libGameCore.so:bss");
  long address =
      ValueLmultiple(libGameCore_startaddr + 0x20D0,
                     new long[5]{0x38, 0x18, 0x30, 0xA8, 0xE0}, (char *)"5");
  for (int k = 0; k < (readInt(address + 0x14) / 2); k++) {
    StaticAddress[k].buff = ValueLmultiple(
        address, new long[3]{(order[pdid - 1][k] * 0x20), 0x2A0, 0x248},
        (char *)"3");
  }
}

void game(int pdid) {
  hero = 0;
  long libGameCore_startaddr =
      get_module_base(target_pid, (char *)"libGameCore.so:bss");
  long address =
      ValueLmultiple(libGameCore_startaddr + 0x20D0,
                     new long[4]{0x38, 0x18, 0x60, 0x58}, (char *)"4");
  for (int i = 0; i < readInt(address + 0x14); i++) {
    long Mainaddress = ValueLmultiple(
        address, new long[3]{(i * 24 + 8), 0x98, 0}, (char *)"3");
    long Xaddr =
        ValueLmultiple(Mainaddress + 0x148, new long[2]{0x10, 0}, (char *)"2");
    if (readInt(Mainaddress + 0x2C) == pdid &&
        (readInt(Xaddr) != 0 || readInt(Xaddr + 0x8) != 0)) {
      StaticAddress[hero].id = Mainaddress;
      hero++;
    }
  }
}

void matrix(int *pdid) {
  long libil2cpp_startaddr =
      get_module_base(target_pid, (char *)"libil2cpp.so:bss");
  matrixaddr = ValueLmultiple(libil2cpp_startaddr + 0xB45888,
                              new long[4]{0xA0, 0, 0x10, 0xC0}, (char *)"4");
  if (readInt(matrixaddr) > 0) {
    *pdid = 2, fx = 1;
  } else {
    *pdid = 1, fx = -1;
  }
}

void print(long state) {
  int pdid = 0;
  matrix(&pdid);
  game(pdid);
  buff(pdid);
  data();
  for (;;) {
    if (readInt(state) == 1) {
      float MatrixMem[16] = {0.0f};
      for (int I = 0; I < 16; I++) {
        MatrixMem[I] = readFloat(matrixaddr + I * 4);
      }
      for (int b = 0; b < 4; b++) {
        DynamicData[b].buffcd = readInt(StaticAddress[b].buff) / 1000;
      }
      for (int i = 0; i < hero; i++) {
        long add = readValueL(StaticAddress[i].id + 0x148);
        int ofint = (readInt(add + 0x28) << 4);
        long XYaddr = readValueL(add + 0x10) + ofint;
        int Xdata = readInt(XYaddr + 0x0);
        int Ydata = readInt(XYaddr + 0x8);
        DynamicData[i].mapX = (Xdata * fx);
        DynamicData[i].mapY = (Ydata * fx);
        DynamicData[i].hp = readInt(StaticAddress[i].hp) * 100.0f /
                            readInt(StaticAddress[i].hp + 0x8);
        Rect rect = matrixmem(Xdata, Ydata, MatrixMem);
        DynamicData[i].entX = rect.X;
        DynamicData[i].enty = rect.Y;
        DynamicData[i].zhscd = readInt(StaticAddress[i].d) / 8192000;
        DynamicData[i].dcd = readInt(StaticAddress[i].zhs) / 8192000;
        printf("英雄id：%d,血量百分比：%lf,地图坐标：%lf,%lf,召唤师技能cd：%d,"
               "大招cd：%d,实体坐标：%lf,%lf,\n",
               DynamicData[i].id, DynamicData[i].hp, DynamicData[i].mapX,
               DynamicData[i].mapY, DynamicData[i].dcd, DynamicData[i].zhscd,
               DynamicData[i].entX, DynamicData[i].enty);
      }
      printf("我方蓝buffcd：%d, 我方红buffcd：%d, 敌方蓝buffcd：%d, "
             "敌方红buffcd：%d,\n",
             DynamicData[0].buffcd, DynamicData[1].buffcd,
             DynamicData[2].buffcd, DynamicData[3].buffcd);
      printf("\n");
      usleep(1000);
    }
    usleep(20000);
  }
}

int main() {
  system("echo 0 > /proc/sys/fs/inotify/max_user_watches");
  pthread_t pthread[1];
  target_pid = findpid("com.tencent.tmgp.sgame");
  if (target_pid == -1) {
    exit(0);
  }
  long state =
      get_module_base(target_pid, (char *)"libGameCore.so:bss") + 0x44D668;
  if (state < 0xFFFFFFFFF) {
    exit(1);
  }
  pthread_create(&(pthread[0]), NULL, &findpid1, NULL);
  getscreen();
  for (;;) {
    if (readInt(state) == 1) {
      usleep(200000);
      print(state);
    }
    usleep(10000);
  }
}
