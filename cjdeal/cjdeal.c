﻿#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <signal.h>
#include <pthread.h>
#include <wait.h>
#include <errno.h>
#include <syslog.h>
#include <sys/mman.h>
#include <sys/reboot.h>
#include <bits/types.h>
#include <bits/sigaction.h>

#include "cjdeal.h"
#include "read485.h"
#include "readplc.h"
#include "guictrl.h"
#include "acs.h"
#include "event.h"
#include "calc.h"
#include "ParaDef.h"
#include "EventObject.h"

ProgramInfo* JProgramInfo=NULL;
int ProIndex=0;
INT8U poweroffon_state = 0; //停上电抄读标志 0无效，1抄读，2抄读完毕
MeterPower MeterPowerInfo[POWEROFFON_NUM]; //当poweroffon_state为1时，抄读其中ERC3106State=1得tsa得停上电时间，
                                           //赋值给结构体中得停上电时间，同时置VALID为1,全部抄完后，置poweroffon_state为2
/*********************************************************
 *程序入口函数-----------------------------------------------------------------------------------------------------------
 *程序退出前处理，杀死其他所有进程 清楚共享内存
 **********************************************************/
void QuitProcess(ProjectInfo *proinfo)
{
	close_named_sem(SEMNAME_SPI0_0);
	proinfo->ProjectID=0;
    fprintf(stderr,"\n退出：%s %d",proinfo->ProjectName,proinfo->ProjectID);
	exit(0);
}
/*******************************************************
 * 清死亡计数
 */
void clearcount(int index) {
    JProgramInfo->Projects[index].WaitTimes = 0;
}
/*********************************************************
 * 进程初始化
 *********************************************************/
int InitPro(ProgramInfo** prginfo, int argc, char *argv[])
{
	if (argc >= 2)
	{
		*prginfo = OpenShMem("ProgramInfo",sizeof(ProgramInfo),NULL);
		ProIndex = atoi(argv[1]);
		fprintf(stderr,"\n%s start",(*prginfo)->Projects[ProIndex].ProjectName);
		(*prginfo)->Projects[ProIndex].ProjectID=getpid();//保存当前进程的进程号
		return 1;
	}
	return 0;
}

/********************************************************
 * 载入档案、参数
 ********************************************************/
int InitPara()
{
	InitACSPara();
	read_oif203_para();		//开关量输入值读取
	return 0;
}
/*********************************************************
 * 主进程
 *********************************************************/
int main(int argc, char *argv[])
{
	pid_t pids[128];
    struct sigaction sa = {};
    Setsig(&sa, QuitProcess);

    if (prog_find_pid_by_name((INT8S*)argv[0], pids) > 1)
		return EXIT_SUCCESS;

	fprintf(stderr,"\n[cjdeal]:cjdeal run!");
	if(InitPro(&JProgramInfo,argc,argv)==0){
		fprintf(stderr,"进程 %s 参数错误",argv[0]);
		return EXIT_FAILURE;
	}

	struct mq_attr attr_485_main;
	mqd_485_main = mmq_open((INT8S *)PROXY_485_MQ_NAME,&attr_485_main,O_RDONLY);

	//载入档案、参数
	InitPara();
	//485、四表合一
	read485_proccess();
	//统计计算 电压合格率 停电事件等
//	calc_proccess();
	//载波
	//readplc_proccess();
	//液晶、控制
	//guictrl_proccess();
	//交采
	//acs_process();

	while(1)
   	{
	    struct timeval start={}, end={};
	    long  interval=0;
		gettimeofday(&start, NULL);
		DealState(JProgramInfo);
		gettimeofday(&end, NULL);
		interval = 1000000*(end.tv_sec - start.tv_sec) + (end.tv_usec - start.tv_usec);
	    if(interval>=1000000)
	    	fprintf(stderr,"deal main interval = %f(ms)\n", interval/1000.0);
		usleep(10 * 1000);
		clearcount(ProIndex);
   	}
	close_named_sem(SEMNAME_SPI0_0);
	return EXIT_SUCCESS;//退出
}
