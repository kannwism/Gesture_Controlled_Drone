

#include "config.h"
#include "adc.h"
#include "control.h"
#include "ConfigTable.h"
#include "Led.h"
#include "NRF24L01.h"
#include "delay.h"
#include "CommUAV.h"


int Throttle;
int Roll;
int Pitch;
int Yaw;

uint16_t Throttle_Calibra=0;
uint16_t Pitch_Calibra   =0;
uint16_t Roll_Calibra    =0;
uint16_t Yaw_Calibra     =0;//摇杆校准值

char Lockflag      = 0;
char IMUcalibratflag = 0;

uint16_t raw_Throttle_c3=0;
uint16_t raw_Pitch_c1   =0;
uint16_t raw_Roll_c0    =0;
uint16_t raw_Yaw_c2     =0;


void LoadRCdata(void)
{
			raw_Throttle_c3 = Get_Adc_Average(3,15);
      Throttle=1500 - (Throttle_Calibra-(1000 + (1000 - (1000*raw_Throttle_c3)/4096)));//采集油门摇杆的位置，由于硬件原因，需要用100-采集值
      Throttle=(Throttle<=1000)?1000:Throttle;               //越界判断
      Throttle=(Throttle>=2000)?2000:Throttle;               //越界判断
			raw_Pitch_c1 = Get_Adc_Average(1,15);
      Pitch= 1500 - (Pitch_Calibra - (1000 + (1000*raw_Pitch_c1)/4096));//采集俯仰摇杆的位置，赋值给对应的俯仰变量
      Pitch=(Pitch<=1000)?1000:Pitch;                 //越界判断
      Pitch=(Pitch>=2000)?2000:Pitch;               //越界判断
			raw_Roll_c0 = Get_Adc_Average(0,15);
      Roll= 1500 - (Roll_Calibra - (1000 + (1000*raw_Roll_c0)/4096));//采集横滚摇杆位置，赋值给对应的横滚变量
      Roll=(Roll<=1000)?1000:Roll;                //越界判断
      Roll=(Roll>=2000)?2000:Roll;              //越界判断
			raw_Yaw_c2= Get_Adc_Average(2,15);
      Yaw= 1500 - (Yaw_Calibra - (1000 + (1000*raw_Yaw_c2)/4096));//采集横滚摇杆位置，赋值给对应的偏航角
      Yaw=(Yaw<=1000)?1000:Yaw;                //越界判断
      Yaw=(Yaw>=2000)?2000:Yaw;              //越界判断
  
}


int8_t ClibraFlag;


void controlClibra(void)
{
	static u8 i;
	uint16_t sum[4]={0,0,0,0};
	static int8_t lednum=1;
  static int8_t clibrasumNum = 20;

	if((ClibraFlag == FAIL))//校准失败
	{

		for(i=0;i<clibrasumNum;i++)
		{
			#ifdef AMERICAN_RC_MODE
			sum[0] += 1000 + (1000 - (1000*Get_Adc_Average(3,15))/4096);
			sum[1] += 1000 + (1000*Get_Adc_Average(1,15))/4096;
			#else
			sum[0] += 1000 + (1000*Get_Adc_Average(1,15))/4096;
			sum[1] += 1000 + (1000 - (1000*Get_Adc_Average(3,15))/4096);
			#endif
			sum[2] += 1000 + (1000*Get_Adc_Average(0,15))/4096;
			sum[3] += 1000 + (1000*Get_Adc_Average(2,15))/4096;
			delay_ms(100);
			
			if(++lednum == led5 + 2)lednum = 2;
			LedSet(lednum - 1,0);
			LedSet(lednum ,1);
		}
		
		Throttle_Calibra = sum[0]/i;
		Pitch_Calibra    = sum[1]/i;
		Roll_Calibra     = sum[2]/i;
		Yaw_Calibra      = sum[3]/i;

		
// 		Throttle_Calibra = 1500;
// 		Pitch_Calibra    = 1500;
// 		Roll_Calibra     = 1500;
// 		Yaw_Calibra      = 1500;

		LoadRCdata();               //摇杆赋值
		if((Throttle>=1510)||(Throttle<1490)||(Pitch>=1510)||(Pitch<=1490)||(Roll>=1510)||(Roll<=1490)||(Yaw>=1510)||(Yaw<=1490))
						ClibraFlag       = FAIL;//校准失败
		else 		ClibraFlag       = OK;//校准成功标志
		
					
		
		//SaveParamsToEEPROM();
		LedSet(led2,0);LedSet(led3,0);LedSet(led4,0);LedSet(led5,0);
	 }	
	
}


static char Locksta = 0xa5;
static u8 cnt = 3;
/****** press key to unlock crazepony****/
void KeyLockcrazepony(void)
{
	u8 i;
	
//	if(0 == ++cnt){
//		//防止数据溢出
//		cnt = 3;
//	}
	
	switch( Lockflag )
	{
		case 1:
				//解决按键连按的毛刺
				//本函数会被主循环10Hz调用，测试发现cnt为1或者2的时候，（200ms以下），属于按键毛刺，应该剔除
//				if(cnt < 3){
//					printf("invalid key press:%d\n",cnt);
//					cnt = 0;
//					Lockflag = 0;
//					break;
//				}else{
//					cnt = 0;
//				}
		
				if(Locksta == 0xa5) 
				{
					for(i=0;i<5;i++)         
						CommUAVUpload(MSP_ARM_IT);   //unlock Crazepony
					Locksta = 0x5a;
					Lockflag = 0;
				}
					
				else if(Locksta == 0x5a )
				{
					for(i=0;i<5;i++)         
					CommUAVUpload(MSP_DISARM_IT);	//lock Crazepony
					Locksta = 0xa5;
					Lockflag = 0;
				}
			break;
		case 0:
			if(Locksta == 0x5a)   LedSet(led5,1);
			else if(Locksta == 0xa5) LedSet(led5,0);
			break;
	}	
}

/****** remote rocker to unlock crazepony****/
void RockerUnlockcrazepony()
{
#ifdef AMERICAN_RC_MODE
									while((Throttle>1050)||(Roll>1050))// thr < 1050,roll < 1050
											 {
												 LoadRCdata();            //
												 CommUAVUpload(MSP_DISARM_IT);
												 //NRF24L01_TxPacket(TxBuf);//9ms
											 }
#else
			            while((Throttle>1050)||(Yaw>1050))// thr < 1050,yaw < 1050
											 {
												 LoadRCdata();            //
												 CommUAVUpload(MSP_DISARM_IT);
												 //NRF24L01_TxPacket(TxBuf);//9ms
											 }
#endif
	
}


/*IMUcalibrate  */
void IMUcalibrate(void)
{
	  LedSet(led4,IMUcalibratflag);
	  if(IMUcalibratflag) 
			{
				CommUAVUpload(MSP_ACC_CALI);
				IMUcalibratflag = 0;
			}
}


/*remote calibrate*/
void Remotecalibrate()
{
		if((ClibraFlag == FAIL)&&
				((Throttle<=1510)&&(Throttle>1490)&&
				(Pitch<=1510)&&(Pitch>=1490)&&
				(Roll<=1510)&&(Roll>=1490)&&
				(Yaw<=1510)&&(Yaw>=1490)))
				controlClibra();	  
}


