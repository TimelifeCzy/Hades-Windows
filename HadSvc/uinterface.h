#pragma once
#include "unet.h"
#include "uetw.h"
#include "ufile.h"
#include "usysuser.h"
#include "usysinfo.h"
#include "uautostart.h"
#include "uprocesstree.h"
#include "uservicesoftware.h"
#include <SingletonHandler.h>

// �����ߵ���
using SingletonUEtw = ustdex::Singleton<UEtw>;
using SingletonUFile = ustdex::Singleton<UFile>;
using SingletonUProcess = ustdex::Singleton<UProcess>;
using SingletonUNetWork = ustdex::Singleton<UNetWork>;
using SingletonNSysUser = ustdex::Singleton<USysUser>;
using SingletonUAutoStart = ustdex::Singleton<UAutoStart>;
using SingletonUServerSoftware = ustdex::Singleton<UServerSoftware>;