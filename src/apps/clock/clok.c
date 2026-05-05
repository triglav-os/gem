/*
 * Implements the clock display and update logic used by the Calclock
 * accessory. The code keeps the time-oriented UI in sync with the
 * accessory resource tree.
 *
 * MIT License (see: LICENSE)
 * Copyright (C) 2026 tomaz stih
 */

#include <gem/portab.h>
#include <machine.h>
#include <obdefs.h>
#include <taddr.h>
#include <crysbind.h>
#include <clok.h>

/* Local constants. */
/* Global state. */
EXTERN WORD	gl_apid;
EXTERN WORD	gl_handle;
EXTERN WORD     color;

EXTERN WORD	gl_spnxt;

GLOBAL LONG	ad_clok;
GLOBAL WORD	wh_clok;
GLOBAL WORD	gl_itclok;	

WORD	last_clok;
WORD	al_min,al_hour;
WORD	done_one;
WORD	al_set;
LONG    TIMEON;
LONG    ALARMON;
 
/*
*	Initializes the clock tree, TEDINFO records, and strings used
*	by the accessory.
*/
LONG ini_clok(VOID)
{
	LONG		tree;
	WORD		i, j;
	BYTE		*ptemp;

	tree = ADDR(&the_clok[0]);
	ptemp = clok_str[0];
        TIMEON = 0x09001100L;
        ALARMON = 0x0a001200L;

	j = 0;
	for (i=0; i < CLOK_OBS; i++)
	{
	  rsrc_obfix(tree, i);
	  if (the_clok[i].ob_type == G_BOXTEXT)
	  {
	    the_clok[i].ob_spec = ADDR(&clok_ted[j]);
	    clok_ted[j].te_ptext = ADDR(clok_str[j]);
	    ptemp = clok_str[j];
            ptemp[0] = ' ';
            ptemp[1] = ' ';
	    j++;
          }
	}
					/* Use alternate object colors when the display supports them. */
	if (color == 2)
	  {	
           TIMEON = 0x09001170L;
           ALARMON = 0x0a001170L;
	   the_clok[THEALARM].ob_state = NORMAL;
	   the_clok[3].ob_spec = 0x09001170L;	
	   the_clok[4].ob_spec = 0x20001273L;	
	   the_clok[6].ob_spec = 0x3a001173L;	
	   the_clok[8].ob_spec = 0x20001173L;	
	   the_clok[10].ob_spec = 0x20001273L;	
	   the_clok[11].ob_spec = 0x0b001170L;	
	   the_clok[14].ob_spec = 0x2f001172L;	
	   the_clok[16].ob_spec = 0x2f001172L;	
	   the_clok[17].ob_spec = 0x20001172L;	
	   clok_ted[0].te_color = 0x1173;
	   clok_ted[1].te_color = 0x1173;
	   clok_ted[2].te_color = 0x1173;
	   clok_ted[3].te_color = 0x1172;
	   clok_ted[4].te_color = 0x1172;
	   clok_ted[5].te_color = 0x1172;
	   clok_ted[6].te_color = 0x1172;
	  }  
 	if (color >= 3)
	  {	
           TIMEON = 0x09001170L;
           ALARMON = 0x0a001270L;
	   the_clok[THEALARM].ob_state = NORMAL;
	   the_clok[3].ob_spec = 0x09001170L;	
	   the_clok[4].ob_spec = 0x20001273L;	
	   the_clok[6].ob_spec = 0x3a001173L;	
	   the_clok[8].ob_spec = 0x20001173L;	
	   the_clok[10].ob_spec = 0x20001273L;	
	   the_clok[11].ob_spec = 0x0b001170L;	
	   the_clok[14].ob_spec = 0x2f001475L;	
	   the_clok[16].ob_spec = 0x2f001475L;	
	   the_clok[17].ob_spec = 0x20001475L;	
	   clok_ted[0].te_color = 0x1173;
	   clok_ted[1].te_color = 0x1173;
	   clok_ted[2].te_color = 0x1173;
	   clok_ted[3].te_color = 0x1175;
	   clok_ted[4].te_color = 0x1175;
	   clok_ted[5].te_color = 0x1175;
	   clok_ted[6].te_color = 0x1175;
	  }  
	return(tree);
}  /* ini_clok */


/*
*		Display the clock object specified by the parmater dsp_obj.
*/

VOID disp_clok(WORD dsp_obj)
{
	WORD		xdsp,ydsp;       			   

	objc_offset(ad_clok, dsp_obj, &xdsp, &ydsp);
	do_redraw(wh_clok,dsp_obj,0,xdsp,ydsp,
  	         the_clok[dsp_obj].ob_width,the_clok[dsp_obj].ob_height);	 
}  /* disp_clok */


/*
*		Decide wether it is AM or PM and display the appropriate
*		one.
*/

VOID show_ampm(VOID)
{
	UWORD		time;
	BYTE		*ptemp;

	ptemp = clok_str[2];
				/* Get the alarm hour if the alarm is */
				/* currently displayed.		      */

	if (the_clok[SW_TIME].ob_spec == ALARMON)
	  time = al_hour;
	 else
	   {
            time = dos_gtime();
            time >>= 8;
	   }
	ptemp[0] = (time > 11) ? 'p' : 'a';
	ptemp[1] = 'm'; 
	disp_clok(AM_PM); 
} 

/*
*		Convert from an integer value to an ascii value.
*/

VOID i_to_a(UWORD ival, WORD zlead, BYTE pstr[])
{
	pstr[0] = (ival / 10) + 48;
	pstr[1] = (ival % 10) + 48;
	if ( (!zlead) &&
	     (pstr[0] == '0') )
	  pstr[0] = ' ';
}


/*
*			Determine the corect monute and display it.
*/
VOID show_min(VOID)
{
	UWORD		time;
	BYTE		*ptemp;

	ptemp = clok_str[1];
	if (the_clok[SW_TIME].ob_spec == ALARMON)
	  time = al_min;
	 else
	   {
	    time = dos_gtime();
	    time &= 0x00ff;
	   }
	i_to_a(time, TRUE, &ptemp[0]);
	disp_clok(MINUTE);  
}


/*
*		Determine the correct hour and display it.
*/

VOID show_hour(VOID)
{
	UWORD		time;
	BYTE		*ptemp;

	ptemp = clok_str[0];
	if (the_clok[SW_TIME].ob_spec == ALARMON)
	  time = al_hour;
	 else
	   {
	    time = dos_gtime();
	    time >>= 8;
	   }
				/* Convert from 0 - 23 scale to 0 - 11 */
	if (time > 11)
	  {
	   if (time > 12)
   	     time -= 12;	
	  }
				/* Convert from 0 - 11 scale to 1 -12 */
	if (time == 0)
	  time = 12;	
	i_to_a(time, FALSE, &ptemp[0]);
  	disp_clok(HOUR);   
} 

/*
*		Determine the correct month and display it.
*/

VOID show_month(VOID)
{
	UWORD		date;
	BYTE		*ptemp;

	ptemp = clok_str[4];
	date = dos_gdate();
	date >>= 8;
	i_to_a(date, FALSE, &ptemp[0]);
	disp_clok(MONTH);    
}

/*
*		Deteemine the correct date and display it.
*/

VOID show_date(VOID)
{
	UWORD		date;
	BYTE		*ptemp;

	ptemp = clok_str[5];
	date = dos_gdate();
	date &= 0x00ff;
	i_to_a(date, FALSE, &ptemp[0]);
  	disp_clok(THEDATE);  
} 


/*
*		Determine the correct year and display it.
*/
VOID show_year(VOID)
{
	UWORD		date;
	BYTE		*ptemp;

	ptemp = clok_str[6];
	date = dos_gyear();
	date = (date - 1900) % 100;
	i_to_a(date, TRUE, &ptemp[0]);
	disp_clok(YEAR);       
}


/*
*		Selects or de-selects the clock value which is to be changed.
*/
VOID light_it(WORD obj, WORD state)
{
	WORD		x,y,w,h;

	x = the_clok[ROOT].ob_x + the_clok[obj].ob_x;
	y = the_clok[ROOT].ob_y + the_clok[obj].ob_y;
	w = the_clok[ROOT].ob_width;
	h = the_clok[ROOT].ob_height;
	objc_change(ad_clok, obj, 0, x, y, w, h, state, TRUE);  
}


/*
*		Selcts the object which is going to be changed, and if 
*		another one of the objects is already selected it de-selecs
*		that object.
*/
VOID hi_light(WORD obj)
{
	WORD		x,y,w,h;
	WORD		ii;

	for (ii=0; ii < CLOK_OBS; ii++)
	{
	  if (the_clok[ii].ob_state == SELECTED)
 	  {
	    light_it(ii, NORMAL);
	    done_one = FALSE;
    	  }
	}
	light_it(obj, SELECTED);
}


/*
*		Takes the user's input and sets the hour to that value.
*		If the value is not a valid hour no change occurs.
*/

VOID set_hour(WORD the_hour, BYTE *ptemp)
{			    
	WORD		tmp_hour;	 
	WORD		no_eror;
	WORD		temp;

          tmp_hour = (((ptemp[0] - 48) * 10) + the_hour) ;
 	  no_eror = !( (tmp_hour < 1) || (tmp_hour > 12) );
	  if (tmp_hour == 12)
            tmp_hour = 0; 
	  if ( no_eror )
	  {
	    if (the_clok[SW_TIME].ob_spec == ALARMON)
	    {
			/* If al_hour > 11 then PM must be displayed, */
			/* convert to 0 -23 scale.		      */
	      if (al_hour > 11)
	        tmp_hour += 12;	
              al_hour = tmp_hour;
	    }	
	    else
	    {	
	      temp = dos_gtime();
	      temp >>= 8;
	      if (temp > 11)
	        tmp_hour += 12;	
              dos_shour(tmp_hour);
	    }	
	  }
	  return(no_eror);
}		



/*
*		Takes the user's input and sets the minute to that value.
*		If the value is not a valid minute no change occurs.
*/

VOID set_min(WORD the_min, BYTE *ptemp)
{			    
	WORD		tmp_min;	 
	WORD		no_eror;

          tmp_min = ((ptemp[0] - 48) * 10) + the_min ;
 	  no_eror = !( (tmp_min < 0) || (tmp_min > 59) );
	  if ( no_eror )
	  {
	    if (the_clok[SW_TIME].ob_spec == ALARMON)
              al_min = tmp_min;
else dos_smin(tmp_min);
	  }
	  return(no_eror);
}

/*
*		Toggles between AM and PM.
*/

VOID set_ampm(VOID)
{
  	WORD		tmp_hour;
	BYTE		*qtemp;

	qtemp = clok_str[2];
	if (the_clok[SW_TIME].ob_spec == ALARMON)
	{
	  al_hour += (al_hour < 12) ? 12 : -12;
	}
	else
	{
          tmp_hour = (dos_gtime() >> 8);
	  tmp_hour += (qtemp[0] == 'a') ? 12 : -12;
	  dos_shour(tmp_hour);
	}
	show_ampm();
}	 



/*
*		Takes the user's input and sets the month to that value.
*		If the value is not a valid month no change occurs.
*/

VOID set_month(WORD the_month, BYTE *ptemp)
{			    
	WORD		tmp_month;	 
	WORD		no_eror;

          tmp_month = ((ptemp[0] - 48) * 10) + the_month ;
 	  no_eror = dos_smonth(tmp_month);
	  return(no_eror);
}		



/*
*		Takes the user's input and sets the date to that value.
*		If the value is not a valid date no change occurs.
*/

VOID set_date(WORD the_day, BYTE *ptemp)
{			    
	WORD		tmp_day;	 
	WORD		no_eror;

          tmp_day = ((ptemp[0] - 48) * 10) + the_day ;
 	  no_eror = dos_sdate(tmp_day);
	  return(no_eror);
}		



/*
*		Takes the user's input and sets the year to that value.
*		If the value is not a valid year no change occurs.
*/

VOID set_year(WORD the_year, BYTE *ptemp)
{			    
	WORD		tmp_year;	 
	WORD		no_eror;

          tmp_year = ((ptemp[0] - 48) * 10) + the_year ;
 	  no_eror = dos_syear(tmp_year);
	  return(no_eror);
}		

/*
*		Determine which object to set, and set it.
*/
WORD set_it(WORD last_inp, WORD value)
{
	WORD		ret;
	WORD		num;
	BYTE		*ptemp;

	switch(last_inp)
	{
	    case THEDATE:	num = 5;	break;
	    case YEAR:		num = 6;	break;
	    case MONTH:		num = 4;	break;
	    case HOUR:		num = 0;	break;
	    case MINUTE:	num = 1;	break;
	}
	ptemp = clok_str[num];
				/* After one digit has been typed */
				/* done one is set to true, and   */
				/* you wait for the second digit. */
	if (!done_one)
        {			       	
	  done_one = TRUE;
	  ptemp[0] = value + 48;	
	  ret = 0;
  	}
	else
	{
	  done_one = FALSE;
	  switch(last_inp)
	  {
	    case THEDATE:
		ret = set_date(value, ptemp);	     
		break;
	    case YEAR:
		ret = set_year(value, ptemp);
		break;
	    case MONTH:
		ret = set_month(value, ptemp);			
		break;
	    case HOUR:
		ret = set_hour(value, ptemp);
		break;
	    case MINUTE:
		ret = set_min(value, ptemp);
		break;
	  }
	  if ( ret )
	    ret = 1;
	  else
	    ret = -1;
	}
	return(ret);
}

/*
*		Show the new value of the object which was changed.
*/

WORD show_it(WORD last_inp)
{

	switch(last_inp)
	{
	  case THEDATE:
		show_date();	     
		break;
	  case YEAR:
		show_year();
		break;
	  case MONTH:
		show_month();			
		break;
	  case HOUR:
		show_hour();
		break;
	  case MINUTE:
		show_min();
		break;
	}
}


/*
*		Processes all of the changes to the clock.
*/
WORD hndl_clok(WORD obj)
{
	BYTE		xdisp,ydisp;
	WORD		ret, value;

	value = 0;
	switch (obj)
	{
	  case THECLOCK:
		break;
	  case THEALARM:
				/* If the alarm icon is chosen, toggle  */
				/* a selected and a normal state.	*/
				/* Use alternate object colors when the display supports them. */
				/* one color the color is toggled be-	*/
				/* red and black.			*/
	        if (color >= 2)
		  {
		   if (the_clok[THEALARM].ob_spec == 0x0b001270L)
		     {
		      al_set = FALSE;	
	              the_clok[THEALARM].ob_spec = 0x0b001170L;
		     }
	            else 
		      {
		       al_set = TRUE;	
		       the_clok[THEALARM].ob_spec = 0x0b001270L;
		      }	
		   }
		if (color == 1)
		  {
		   if (!al_set)
		     {
		      al_set = TRUE;
		      the_clok[THEALARM].ob_state = NORMAL;   
		     }
		    else 
		      {
		       al_set = FALSE;
		       the_clok[THEALARM].ob_state = DISABLED;	
		      }	
		   }  
		disp_clok(THEALARM);
		break;
	  case SW_TIME:
				/* If the time/alarm flag is chosen the */
				/* clock will toggle between displaying	*/
				/* the time and the alarm.		*/

		if (the_clok[SW_TIME].ob_spec == TIMEON)
		  the_clok[SW_TIME].ob_spec = ALARMON;
		else
		  the_clok[SW_TIME].ob_spec = TIMEON;
		disp_clok(SW_TIME);
		show_hour();
		show_min();
		show_ampm();
		break;			 
	  case AM_PM:
		set_ampm();
		break;
	  case YEAR:
	  case MONTH:
	  case THEDATE:
	  case HOUR:	    
	  case MINUTE:	  
				/* Highlight the object selected and	*/
				/* for the numeric input.		*/
		hi_light(obj);
		last_clok = obj;
		break;	    
	  case '9':
		value++;
	  case '8':
		value++;
	  case '7':
		value++;
	  case '6':
	  	value++;
	  case '5':
		value++;
	  case '4':
		value++;
	  case '3':
		value++;
	  case '2':
		value++;
	  case '1':
		value++;
	  case '0':
				/* Set the clock object specified by 	*/
				/* last_inp with the value entered.	*/
		ret = set_it(last_clok, value);
		if (ret != 0)
		{
		  light_it(last_clok, NORMAL);
				/* If no error occured show the new	*/
				/* else beep.				*/
		  if (ret == 1)
		    show_it(last_clok);
		  if (ret == -1)
		    beep();
		}		
		break;		
	 }
	return(FALSE);
}
				
/*
*		Determine wether the alarm time is the same as the
*		system time and if so ring the bell.
*/

VOID bell(VOID)
{

	UWORD		time_hour,time_min;
	WORD		jj;  

	time_hour = dos_gtime();
	time_min = (time_hour & 0x00ff);
	time_hour >>= 8;		   
	if (time_min == al_min) 
	  if (time_hour == al_hour)  
	    for (jj = 0;jj <= 4;jj++)
	      beep();   
}
  
/*
*		Determine how many milli-seconds until the next minute.
*		and return that value.
*/
LONG set_timer(VOID)
{
	LONG		time_left;

	if (gl_spnxt)
	  return( 0x0L );

	time_left = dos_gsec();
	time_left >>= 8;
	time_left = (60 - (time_left + 0)) * 1000;
	return(time_left);
}	  			

/*
*		Check to see if the time is currently being displayed.
*		if so display the new time.If the alarm is set check to
*		see if it is time to ring the alarm.
*/

VOID change_time(VOID)
{
           if (wh_clok)
           {
	     if (the_clok[SW_TIME].ob_spec == TIMEON)
	     {
	       evnt_timer(500L);
	       show_hour();
	       show_min();
	       show_ampm();
	       show_month();
	       show_date();
	       show_year();
	    }				       
 	  }                           
	  if (al_set)
	    bell();  
}
