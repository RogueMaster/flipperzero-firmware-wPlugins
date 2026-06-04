#include <furi.h>
#include <furi_hal.h>
#include <gui/gui.h>
#include <input/input.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <storage/storage.h>
#include <expansion/expansion.h>
#include <notification/notification_messages.h>

#include "gps_uart.h"
#include "constants.h"

#define TRACK_BASE        "/ext/apps_data"
#define TRACK_DIR         TRACK_BASE "/gps_track"
#define TRACK_FILE        TRACK_DIR "/track.csv"
#define TRACK_STATE_FILE  TRACK_DIR "/state.txt"
#define SETTINGS_FILE     TRACK_DIR "/settings.csv"
#define POI_FILE          TRACK_DIR "/poi.csv"
#define AREA_FILE         TRACK_DIR "/area.csv"
#define TRACK_MAX_PTS     1440
#define POI_MAX           50
#define ARRIVE_DIST_M     30.0f
#define ARRIVE_NEAR_M     60.0f
#define ARRIVE_NEAR_SECS  30
#define MIN_CRUMB_DIST_M  50.0f
#define SPEED_BUF_SIZE    600
#define MOVING_SPEED_MS   0.5f
#define NAME_MAX          12
#define COURSE_SMOOTH     3
#define AREA_MAX_VERTS    30
#define AREA_SMOOTH_N     5
#define AREA_MIN_DIST_M   2.0f

typedef enum { POI_ICON_PIN, POI_ICON_HOME } PoiIcon;
typedef struct { char name[NAME_MAX+1]; float lat; float lon; PoiIcon icon; } PoiPoint;
typedef struct { float lat; float lon; char time[6]; } TrackPoint;

#define COORD_LAT_LEN 10
#define COORD_LON_LEN 11
#define COORD_TOTAL   (COORD_LAT_LEN + COORD_LON_LEN)

typedef struct {
    GpsUart*   gps_uart;
    TrackPoint points[TRACK_MAX_PTS];
    int        point_count;
    int        crumbs_target;
    bool       crumbs_show_total;
    float      total_track_dist;
    float      passed_track_dist;
    float      remaining_track_dist;
    bool       crumbs_reset_on_next_entry;
    PoiPoint   poi[POI_MAX];
    int        poi_count;
    int        poi_target;
    bool       nav_show_eta;
    float      speed_buf[SPEED_BUF_SIZE];
    int        speed_buf_pos;
    int        speed_buf_filled;
    float      course_buf[COURSE_SMOOTH];
    int        course_buf_pos;
    int        course_buf_filled;
    float      smoothed_course;
    uint32_t   near_target_ticks;
    int        overlay_ticks;
    char       overlay_msg[48];
    int        poi_list_scroll;
    int        crumbs_menu_sel;
    bool       crumbs_clear_confirm;
    int        poi_delete_idx;
    bool       poi_delete_confirm;
    int        poi_rename_idx;
    char       coord_lat_str[COORD_LAT_LEN+1];
    char       coord_lon_str[COORD_LON_LEN+1];
    int        coord_cursor;
    char       name_input_buf[NAME_MAX+1];
    int        name_input_cursor;
    PoiIcon    name_input_icon;
    bool       name_input_has_fix;
    float      name_input_lat;
    float      name_input_lon;
    bool       name_input_is_rename;
    bool       name_input_active;
    float      last_lat;
    float      last_lon;
    float      last_course;
    bool       had_fix;
    uint32_t   first_fix_tick;
    bool       first_fix_done;
    bool       first_point_written;
    uint32_t   last_save_tick;
    uint32_t   tick_count;
    bool       led_on;
    int        led_white_ticks;
    int        settings_sel;
    bool       settings_dirty;
    SpeedUnit  settings_snap_speed;
    int        settings_snap_tz;
    int        settings_snap_baudrate_idx;
    bool       settings_snap_deepsleep;
    // area calculation
    float      area_poly_lat[AREA_MAX_VERTS];
    float      area_poly_lon[AREA_MAX_VERTS];
    int        area_poly_count;
    bool       area_active;
    bool       area_clear_confirm;
    float      area_m2;
    float      area_smooth_lat[AREA_SMOOTH_N];
    float      area_smooth_lon[AREA_SMOOTH_N];
    int        area_smooth_pos;
    int        area_smooth_count;
} AppState;

typedef enum { EvtTypeTick, EvtTypeInput } EvtType;
typedef struct { EvtType type; InputEvent input; } AppEvent;

#define DEG2RAD(d) ((d)*3.14159265358979f/180.0f)
#define RAD2DEG    (180.0f/3.14159265358979f)

static float haversine_m(float la1,float lo1,float la2,float lo2){
    float dlat=DEG2RAD(la2-la1),dlon=DEG2RAD(lo2-lo1);
    float a=sinf(dlat/2)*sinf(dlat/2)+cosf(DEG2RAD(la1))*cosf(DEG2RAD(la2))*sinf(dlon/2)*sinf(dlon/2);
    return 6371000.0f*2.0f*atan2f(sqrtf(a),sqrtf(1.0f-a));
}
static float bearing_deg(float la1,float lo1,float la2,float lo2){
    float dlon=DEG2RAD(lo2-lo1);
    float y=sinf(dlon)*cosf(DEG2RAD(la2));
    float x=cosf(DEG2RAD(la1))*sinf(DEG2RAD(la2))-sinf(DEG2RAD(la1))*cosf(DEG2RAD(la2))*cosf(dlon);
    return fmodf(atan2f(y,x)*RAD2DEG+360.0f,360.0f);
}
static float avg_speed_ms(AppState*app){
    int n=app->speed_buf_filled<SPEED_BUF_SIZE?app->speed_buf_filled:SPEED_BUF_SIZE;
    if(n==0)return 0.0f;
    float s=0.0f;for(int i=0;i<n;i++)s+=app->speed_buf[i];return s/n;
}
static float course_smooth_calc(AppState*app){
    int n=app->course_buf_filled<COURSE_SMOOTH?app->course_buf_filled:COURSE_SMOOTH;
    if(n==0)return 0.0f;
    float sx=0.0f,sy=0.0f;
    for(int i=0;i<n;i++){float r=DEG2RAD(app->course_buf[i]);sx+=cosf(r);sy+=sinf(r);}
    return fmodf(RAD2DEG*atan2f(sy/n,sx/n)+360.0f,360.0f);
}
static float wrap180(float a){while(a>180)a-=360;while(a<-180)a+=360;return a;}
static void fmt_speed(char*buf,size_t sz,float kn,SpeedUnit u){
    switch(u){
    case MS:    snprintf(buf,sz,"%.1f m/s",(double)(kn*KNOTS_TO_MS));  break;
    case KPH:   snprintf(buf,sz,"%.1f km/h",(double)(kn*KNOTS_TO_KPH));break;
    case MPH:   snprintf(buf,sz,"%.1f mi/h",(double)(kn*KNOTS_TO_MPH));break;
    case KNOTS: snprintf(buf,sz,"%.1f kn",(double)kn);                 break;
    }
}
static const char*speed_unit_name(SpeedUnit u){
    switch(u){case MS:return"m/s";case KPH:return"km/h";case MPH:return"mi/h";case KNOTS:return"kn";}
    return"m/s";
}
static void fmt_dist(char*buf,size_t sz,float m){
    if(m<1000.0f)snprintf(buf,sz,"%.0fm",(double)m);
    else snprintf(buf,sz,"%.2fkm",(double)(m/1000.0f));
}

// ─── storage ──────────────────────────────────────────────────────────────────
static void ensure_dir(Storage*s){
    storage_simply_mkdir(s,TRACK_BASE);
    storage_simply_mkdir(s,TRACK_DIR);
}
static void nav_update_dists(AppState*app);

static void settings_save(GpsUart*gu,Storage*storage){
    ensure_dir(storage);
    File*f=storage_file_alloc(storage);
    if(storage_file_open(f,SETTINGS_FILE,FSAM_WRITE,FSOM_CREATE_ALWAYS)){
        char buf[64];
        snprintf(buf,sizeof(buf),"%d,%d,%d,%d\n",(int)gu->speed_units,gu->tz_offset,current_gps_baudrate,gu->deep_sleep_enabled?1:0);
        storage_file_write(f,buf,strlen(buf));storage_file_close(f);
    }
    storage_file_free(f);
}
static void settings_load(GpsUart*gu,Storage*storage){
    File*f=storage_file_alloc(storage);
    if(storage_file_open(f,SETTINGS_FILE,FSAM_READ,FSOM_OPEN_EXISTING)){
        char buf[64];uint32_t rd=storage_file_read(f,buf,sizeof(buf)-1);buf[rd]='\0';
        int su=0,tz=0,baud=1,ds=0;sscanf(buf,"%d,%d,%d,%d",&su,&tz,&baud,&ds);
        gu->speed_units=(SpeedUnit)su;gu->tz_offset=tz;
        if(baud>=0&&baud<6){current_gps_baudrate=baud;gu->baudrate=(uint32_t)gps_baudrates[baud];}
        gu->deep_sleep_enabled=(ds!=0);storage_file_close(f);
    }
    storage_file_free(f);
}
static void track_load(AppState*app,Storage*storage){
    app->point_count=0;
    File*f=storage_file_alloc(storage);
    if(!storage_file_open(f,TRACK_FILE,FSAM_READ,FSOM_OPEN_EXISTING)){storage_file_free(f);return;}
    char line[80];int pos=0;uint8_t ch;
    while(storage_file_read(f,&ch,1)==1&&app->point_count<TRACK_MAX_PTS){
        if(ch=='\n'){
            line[pos]='\0';pos=0;
            char*c1=strchr(line,',');if(!c1)continue;*c1='\0';float lat=strtof(line,NULL);
            char*c2=strchr(c1+1,',');float lon;char ts[6]="";
            if(c2){*c2='\0';lon=strtof(c1+1,NULL);strncpy(ts,c2+1,5);ts[5]='\0';}
            else lon=strtof(c1+1,NULL);
            if(lat!=0.0f||lon!=0.0f){
                app->points[app->point_count].lat=lat;app->points[app->point_count].lon=lon;
                strncpy(app->points[app->point_count].time,ts,5);app->point_count++;
            }
            *c1=',';if(c2)*c2=',';
        }else if(pos<(int)sizeof(line)-1)line[pos++]=(char)ch;
    }
    storage_file_close(f);storage_file_free(f);
    app->total_track_dist=0.0f;
    for(int i=app->point_count-1;i>0;i--)
        app->total_track_dist+=haversine_m(app->points[i].lat,app->points[i].lon,app->points[i-1].lat,app->points[i-1].lon);
    nav_update_dists(app);
}
static void poi_load(AppState*app,Storage*storage){
    app->poi_count=0;
    File*f=storage_file_alloc(storage);
    if(!storage_file_open(f,POI_FILE,FSAM_READ,FSOM_OPEN_EXISTING)){storage_file_free(f);return;}
    char line[80];int pos=0;uint8_t ch;
    while(storage_file_read(f,&ch,1)==1&&app->poi_count<POI_MAX){
        if(ch=='\n'){
            line[pos]='\0';pos=0;
            char*p1=strchr(line,',');if(!p1)continue;*p1='\0';int icon=atoi(line);
            char*p2=strchr(p1+1,',');if(!p2){*p1=',';continue;}*p2='\0';float lat=strtof(p1+1,NULL);
            char*p3=strchr(p2+1,',');if(!p3){*p1=',';*p2=',';continue;}*p3='\0';float lon=strtof(p2+1,NULL);
            PoiPoint*pt=&app->poi[app->poi_count];
            pt->icon=(PoiIcon)icon;pt->lat=lat;pt->lon=lon;
            strncpy(pt->name,p3+1,NAME_MAX);pt->name[NAME_MAX]='\0';app->poi_count++;
            *p1=',';*p2=',';*p3=',';
        }else if(pos<(int)sizeof(line)-1)line[pos++]=(char)ch;
    }
    storage_file_close(f);storage_file_free(f);
}
static void poi_save(AppState*app,Storage*storage){
    ensure_dir(storage);
    File*f=storage_file_alloc(storage);
    if(!storage_file_open(f,POI_FILE,FSAM_WRITE,FSOM_CREATE_ALWAYS)){storage_file_free(f);return;}
    char line[80];
    for(int i=0;i<app->poi_count;i++){
        snprintf(line,sizeof(line),"%d,%.6f,%.6f,%s\n",(int)app->poi[i].icon,(double)app->poi[i].lat,(double)app->poi[i].lon,app->poi[i].name);
        storage_file_write(f,line,strlen(line));
    }
    storage_file_close(f);storage_file_free(f);
}
static void state_save(AppState*app,Storage*storage){
    ensure_dir(storage);
    File*f=storage_file_alloc(storage);
    if(storage_file_open(f,TRACK_STATE_FILE,FSAM_WRITE,FSOM_CREATE_ALWAYS)){
        char buf[32];snprintf(buf,sizeof(buf),"%d,%d\n",app->crumbs_target,app->poi_target);
        storage_file_write(f,buf,strlen(buf));storage_file_close(f);
    }
    storage_file_free(f);
}
static void state_load(AppState*app,Storage*storage){
    File*f=storage_file_alloc(storage);
    if(storage_file_open(f,TRACK_STATE_FILE,FSAM_READ,FSOM_OPEN_EXISTING)){
        char buf[32];uint32_t rd=storage_file_read(f,buf,sizeof(buf)-1);buf[rd]='\0';
        int ct=0,pt=-1;sscanf(buf,"%d,%d",&ct,&pt);
        app->crumbs_target=ct;app->poi_target=pt;storage_file_close(f);
    }
    storage_file_free(f);
}
static void track_write_point(float lat,float lon,int hh,int mm,Storage*storage){
    ensure_dir(storage);
    char line[80];snprintf(line,sizeof(line),"%.6f,%.6f,%02d:%02d\n",(double)lat,(double)lon,hh,mm);
    File*fw=storage_file_alloc(storage);
    bool ok=storage_file_open(fw,TRACK_FILE,FSAM_WRITE,FSOM_OPEN_APPEND);
    if(!ok){storage_file_close(fw);ok=storage_file_open(fw,TRACK_FILE,FSAM_WRITE,FSOM_CREATE_ALWAYS);}
    if(ok){storage_file_write(fw,line,strlen(line));storage_file_close(fw);}
    else storage_file_close(fw);
    storage_file_free(fw);
}
static void track_delete_point(AppState*app,int idx,Storage*storage){
    if(idx<0||idx>=app->point_count)return;
    for(int i=idx;i<app->point_count-1;i++)app->points[i]=app->points[i+1];
    app->point_count--;
    ensure_dir(storage);
    File*f=storage_file_alloc(storage);
    if(storage_file_open(f,TRACK_FILE,FSAM_WRITE,FSOM_CREATE_ALWAYS)){
        char line[80];
        for(int i=0;i<app->point_count;i++){
            snprintf(line,sizeof(line),"%.6f,%.6f,%s\n",(double)app->points[i].lat,(double)app->points[i].lon,app->points[i].time);
            storage_file_write(f,line,strlen(line));
        }
        storage_file_close(f);
    }
    storage_file_free(f);
}

// ─── nav helpers ──────────────────────────────────────────────────────────────
static void nav_update_dists(AppState*app){
    app->passed_track_dist=0.0f;
    for(int i=app->point_count-1;i>app->crumbs_target;i--)
        app->passed_track_dist+=haversine_m(app->points[i].lat,app->points[i].lon,app->points[i-1].lat,app->points[i-1].lon);
    app->remaining_track_dist=0.0f;
    for(int i=app->crumbs_target;i>0;i--)
        app->remaining_track_dist+=haversine_m(app->points[i].lat,app->points[i].lon,app->points[i-1].lat,app->points[i-1].lon);
}
static void crumbs_target_reset(AppState*app){
    app->crumbs_target=(app->point_count>0)?app->point_count-1:0;
    nav_update_dists(app);app->near_target_ticks=0;
}
static void led_crumb_switch(AppState*app);
static void crumbs_check_arrive(AppState*app){
    if(app->point_count==0||app->crumbs_target<=0)return;
    GpsStatus*st=&app->gps_uart->status;
    if(!st->valid&&st->fix_quality==0)return;
    TrackPoint*dest=&app->points[app->crumbs_target];
    float d=haversine_m(st->latitude,st->longitude,dest->lat,dest->lon);
    if(d<=ARRIVE_DIST_M){
        app->crumbs_target--;if(app->crumbs_target<0)app->crumbs_target=0;
        nav_update_dists(app);app->near_target_ticks=0;led_crumb_switch(app);
    }else if(d<=ARRIVE_NEAR_M){
        app->near_target_ticks++;
        if(app->near_target_ticks>=ARRIVE_NEAR_SECS){
            app->crumbs_target--;if(app->crumbs_target<0)app->crumbs_target=0;
            nav_update_dists(app);app->near_target_ticks=0;led_crumb_switch(app);
        }
    }else app->near_target_ticks=0;
}
static void poi_sort_by_dist(AppState*app,float my_lat,float my_lon){
    for(int i=1;i<app->poi_count;i++){
        PoiPoint tmp=app->poi[i];float d=haversine_m(my_lat,my_lon,tmp.lat,tmp.lon);int j=i-1;
        while(j>=0&&haversine_m(my_lat,my_lon,app->poi[j].lat,app->poi[j].lon)>d){app->poi[j+1]=app->poi[j];j--;}
        app->poi[j+1]=tmp;
    }
}
static void poi_add(AppState*app,Storage*storage,const char*name,PoiIcon icon,float lat,float lon){
    if(lat==0.0f&&lon==0.0f){strncpy(app->overlay_msg,"Zero coords!",sizeof(app->overlay_msg)-1);app->overlay_ticks=3;return;}
    for(int i=0;i<app->poi_count;i++){
        if(strcmp(app->poi[i].name,name)==0){
            app->poi[i].lat=lat;app->poi[i].lon=lon;app->poi[i].icon=icon;
            poi_save(app,storage);snprintf(app->overlay_msg,sizeof(app->overlay_msg),"Updated: %s",name);app->overlay_ticks=3;return;
        }
    }
    if(app->poi_count>=POI_MAX){strncpy(app->overlay_msg,"POI full! Delete first",sizeof(app->overlay_msg)-1);app->overlay_ticks=4;return;}
    PoiPoint*p=&app->poi[app->poi_count++];p->lat=lat;p->lon=lon;p->icon=icon;
    strncpy(p->name,name,NAME_MAX);p->name[NAME_MAX]='\0';
    poi_save(app,storage);snprintf(app->overlay_msg,sizeof(app->overlay_msg),"Saved: %s",name);app->overlay_ticks=3;
}

// ─── area calculation ─────────────────────────────────────────────────────────
static float area_calc_m2(AppState*app){
    int n=app->area_poly_count;if(n<3)return 0.0f;
    float lat0=app->area_poly_lat[0],lon0=app->area_poly_lon[0];
    float sx=cosf(DEG2RAD(lat0))*111319.9f,sy=111319.9f;
    float sum=0.0f;
    for(int i=0;i<n;i++){
        int j=(i+1)%n;
        float xi=(app->area_poly_lon[i]-lon0)*sx,yi=(app->area_poly_lat[i]-lat0)*sy;
        float xj=(app->area_poly_lon[j]-lon0)*sx,yj=(app->area_poly_lat[j]-lat0)*sy;
        sum+=xi*yj-xj*yi;
    }
    return fabsf(sum)*0.5f;
}
static float area_tri_contrib(AppState*app,int i){
    int n=app->area_poly_count;if(n<3)return 0.0f;
    int prev=(i-1+n)%n,next=(i+1)%n;
    float lat0=app->area_poly_lat[0],lon0=app->area_poly_lon[0];
    float sx=cosf(DEG2RAD(lat0))*111319.9f,sy=111319.9f;
    float x0=(app->area_poly_lon[prev]-lon0)*sx,y0=(app->area_poly_lat[prev]-lat0)*sy;
    float x1=(app->area_poly_lon[i]-lon0)*sx,    y1=(app->area_poly_lat[i]-lat0)*sy;
    float x2=(app->area_poly_lon[next]-lon0)*sx,  y2=(app->area_poly_lat[next]-lat0)*sy;
    return fabsf((x1-x0)*(y2-y0)-(x2-x0)*(y1-y0))*0.5f;
}
static void area_evict_min(AppState*app){
    int n=app->area_poly_count;if(n<4)return;
    int min_idx=1;float min_val=area_tri_contrib(app,1);
    for(int i=2;i<n-1;i++){float v=area_tri_contrib(app,i);if(v<min_val){min_val=v;min_idx=i;}}
    for(int i=min_idx;i<n-1;i++){app->area_poly_lat[i]=app->area_poly_lat[i+1];app->area_poly_lon[i]=app->area_poly_lon[i+1];}
    app->area_poly_count--;
}
static void area_add_vertex(AppState*app,float lat,float lon){
    if(app->area_poly_count>0){
        int last=app->area_poly_count-1;
        if(haversine_m(lat,lon,app->area_poly_lat[last],app->area_poly_lon[last])<AREA_MIN_DIST_M)return;
    }
    if(app->area_poly_count>=AREA_MAX_VERTS)area_evict_min(app);
    app->area_poly_lat[app->area_poly_count]=lat;
    app->area_poly_lon[app->area_poly_count]=lon;
    app->area_poly_count++;
    app->area_m2=area_calc_m2(app);
}
static void area_save_file(AppState*app,Storage*storage){
    if(app->area_poly_count==0)return;
    ensure_dir(storage);
    File*f=storage_file_alloc(storage);
    if(storage_file_open(f,AREA_FILE,FSAM_WRITE,FSOM_CREATE_ALWAYS)){
        char line[48];
        for(int i=0;i<app->area_poly_count;i++){
            snprintf(line,sizeof(line),"%.6f,%.6f\n",(double)app->area_poly_lat[i],(double)app->area_poly_lon[i]);
            storage_file_write(f,line,strlen(line));
        }
        storage_file_close(f);
    }
    storage_file_free(f);
}
static void area_tick(AppState*app,float lat,float lon){
    if(!app->area_active||(lat==0.0f&&lon==0.0f))return;
    app->area_smooth_lat[app->area_smooth_pos%AREA_SMOOTH_N]=lat;
    app->area_smooth_lon[app->area_smooth_pos%AREA_SMOOTH_N]=lon;
    app->area_smooth_pos++;
    if(app->area_smooth_count<AREA_SMOOTH_N)app->area_smooth_count++;
    if(app->area_smooth_pos%AREA_SMOOTH_N!=0)return;
    float alat=0,alon=0;int n=app->area_smooth_count;
    for(int i=0;i<n;i++){alat+=app->area_smooth_lat[i];alon+=app->area_smooth_lon[i];}
    area_add_vertex(app,alat/n,alon/n);
}

// ─── LED ──────────────────────────────────────────────────────────────────────
static void led_crumb_switch(AppState*app){app->led_white_ticks=1;}
static void led_set_color(NotificationApp*n,float bearing_err,bool white){
    if(white){notification_message_block(n,&sequence_blink_white_100);return;}
    float e=bearing_err<0?-bearing_err:bearing_err;
    if(e<15.0f)      notification_message_block(n,&sequence_blink_green_100);
    else if(e<45.0f) notification_message_block(n,bearing_err<0?&sequence_blink_yellow_100:&sequence_blink_cyan_100);
    else if(e<90.0f) notification_message_block(n,bearing_err<0?&sequence_blink_magenta_100:&sequence_blink_blue_100);
    else             notification_message_block(n,&sequence_blink_red_100);
}
static void led_off(NotificationApp*n){notification_message_block(n,&sequence_reset_rgb);}
static void led_update(AppState*app,AppView view,float cur_lat,float cur_lon,
                       float speed_ms,float target_lat,float target_lon,float my_course){
    NotificationApp*n=app->gps_uart->notifications;
    if(furi_hal_power_is_charging()){if(app->led_on){led_off(n);app->led_on=false;}app->led_white_ticks=0;return;}
    if(view!=VIEW_NAV&&view!=VIEW_CRUMBS){if(app->led_on){led_off(n);app->led_on=false;}app->led_white_ticks=0;return;}
    if(target_lat==0.0f&&target_lon==0.0f){if(app->led_on){led_off(n);app->led_on=false;}return;}
    if(app->led_white_ticks>0){app->led_white_ticks--;led_set_color(n,0,true);app->led_on=true;return;}
    if(speed_ms<MOVING_SPEED_MS){if(app->led_on){led_off(n);app->led_on=false;}return;}
    if(cur_lat==0.0f&&cur_lon==0.0f){if(app->led_on){led_off(n);app->led_on=false;}return;}
    float az=bearing_deg(cur_lat,cur_lon,target_lat,target_lon);
    float be=wrap180(az-my_course);
    led_set_color(n,be,false);app->led_on=true;
}

// ─── coord input ──────────────────────────────────────────────────────────────
static void coord_from_float(float lat,float lon,char*ls,char*lo){
    snprintf(ls,COORD_LAT_LEN+1,"%+010.6f",(double)lat);
    snprintf(lo,COORD_LON_LEN+1,"%+011.6f",(double)lon);
}
static float coord_lat_to_float(const char*s){return strtof(s,NULL);}
static float coord_lon_to_float(const char*s){return strtof(s,NULL);}
static int coord_cursor_to_row(int cursor){return cursor<COORD_LAT_LEN?0:1;}
static int coord_cursor_to_col(int cursor){return cursor<COORD_LAT_LEN?cursor:cursor-COORD_LAT_LEN;}
static int coord_next_cursor(int cursor,int dir){
    int c=cursor;
    for(int a=0;a<COORD_TOTAL;a++){
        c=(c+dir+COORD_TOTAL)%COORD_TOTAL;
        int row=coord_cursor_to_row(c);int col=coord_cursor_to_col(c);
        int dot=(row==0)?3:4;if(col!=dot)return c;
    }
    return cursor;
}
static void coord_change_char(AppState*app,int dir){
    int row=coord_cursor_to_row(app->coord_cursor);int col=coord_cursor_to_col(app->coord_cursor);
    char*str=(row==0)?app->coord_lat_str:app->coord_lon_str;int len=(row==0)?COORD_LAT_LEN:COORD_LON_LEN;
    if(col>=len)return;
    char c=str[col];
    if(col==0){str[col]=(c=='+')?'-':'+';return;}
    if(c=='.')return;
    if(c>='0'&&c<='9'){str[col]=(char)((c-'0'+dir+10)%10+'0');return;}
}

// ─── name input ───────────────────────────────────────────────────────────────
static const char NAME_CHARS[]="ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789- ";
#define NAME_CHARS_LEN 38
static char name_next_char(char cur,int dir){
    int idx=0;for(int i=0;i<NAME_CHARS_LEN;i++){if(NAME_CHARS[i]==cur){idx=i;break;}}
    return NAME_CHARS[(idx+dir+NAME_CHARS_LEN)%NAME_CHARS_LEN];
}
static void name_input_start(AppState*app,GpsUart*gu,const char*default_name,PoiIcon icon,bool is_rename){
    if(is_rename){app->name_input_has_fix=true;app->name_input_lat=0;app->name_input_lon=0;}
    else{
        float lat=gu->status.latitude,lon=gu->status.longitude;
        if((lat==0.0f&&lon==0.0f)&&app->had_fix){lat=app->last_lat;lon=app->last_lon;}
        app->name_input_has_fix=app->had_fix&&(lat!=0.0f||lon!=0.0f);
        app->name_input_lat=lat;app->name_input_lon=lon;
    }
    app->name_input_icon=icon;app->name_input_is_rename=is_rename;
    app->name_input_active=false;app->name_input_cursor=0;
    memset(app->name_input_buf,0,sizeof(app->name_input_buf));
    strncpy(app->name_input_buf,default_name,NAME_MAX);
    int len=(int)strlen(app->name_input_buf);
    for(int i=len;i<NAME_MAX&&i<(int)sizeof(app->name_input_buf)-1;i++)app->name_input_buf[i]=' ';
    gu->view_state=is_rename?VIEW_POI_RENAME:VIEW_NAME_INPUT;
}
static void name_input_save(AppState*app,Storage*storage,GpsUart*gu){
    int len=(int)strlen(app->name_input_buf);
    while(len>0&&app->name_input_buf[len-1]==' ')len--;
    app->name_input_buf[len]='\0';
    if(len==0)strncpy(app->name_input_buf,"POI",sizeof(app->name_input_buf)-1);
    if(app->name_input_is_rename){
        if(app->poi_rename_idx>=0&&app->poi_rename_idx<app->poi_count){
            strncpy(app->poi[app->poi_rename_idx].name,app->name_input_buf,NAME_MAX);
            poi_save(app,storage);snprintf(app->overlay_msg,sizeof(app->overlay_msg),"Renamed: %s",app->name_input_buf);app->overlay_ticks=3;
        }
        gu->view_state=VIEW_NAV_LIST;
    }else{
        poi_add(app,storage,app->name_input_buf,app->name_input_icon,app->name_input_lat,app->name_input_lon);
        gu->view_state=VIEW_NORMAL;
    }
}

static AppView ring_right(AppView cur){
    if(cur==VIEW_NORMAL)return VIEW_NAV;
    if(cur==VIEW_NAV)return VIEW_CRUMBS;
    if(cur==VIEW_CRUMBS)return VIEW_NORMAL;
    return VIEW_NORMAL;
}
static AppView ring_left(AppView cur){
    if(cur==VIEW_NORMAL)return VIEW_CRUMBS;
    if(cur==VIEW_CRUMBS)return VIEW_NAV;
    if(cur==VIEW_NAV)return VIEW_NORMAL;
    return VIEW_NORMAL;
}

// ─── draw helpers ─────────────────────────────────────────────────────────────
static void draw_teardrop(Canvas*c,int x,int y){
    canvas_draw_circle(c,x,y-3,3);canvas_draw_line(c,x,y-1,x,y);
    canvas_draw_dot(c,x,y-3);canvas_draw_dot(c,x-1,y-3);canvas_draw_dot(c,x+1,y-3);
    canvas_draw_dot(c,x,y-4);canvas_draw_dot(c,x,y-2);
}
static void draw_flag_filled(Canvas*c,int x,int y){
    canvas_draw_line(c,x,y,x,y-8);for(int i=0;i<5;i++)canvas_draw_line(c,x,y-8+i,x+5-i,y-8+i);
}
static void draw_flag_empty(Canvas*c,int x,int y){
    canvas_draw_line(c,x,y,x,y-8);canvas_draw_line(c,x,y-8,x+5,y-6);canvas_draw_line(c,x+5,y-6,x,y-4);
}
static void draw_pin(Canvas*c,int x,int y){canvas_draw_circle(c,x,y-5,3);canvas_draw_line(c,x,y-2,x,y);}
static void draw_arrow(Canvas*c,int cx,int cy,int r,float angle_deg){
    float rad=DEG2RAD(angle_deg);
    int tx=cx+(int)(sinf(rad)*r),ty=cy-(int)(cosf(rad)*r);
    int bx=cx-(int)(sinf(rad)*r*0.5f),by=cy+(int)(cosf(rad)*r*0.5f);
    canvas_draw_line(c,bx,by,tx,ty);
    float left=rad-3.14159f*0.75f,right=rad+3.14159f*0.75f;
    canvas_draw_line(c,tx,ty,tx+(int)(sinf(left)*r*0.4f),ty-(int)(cosf(left)*r*0.4f));
    canvas_draw_line(c,tx,ty,tx+(int)(sinf(right)*r*0.4f),ty-(int)(cosf(right)*r*0.4f));
}
static void draw_compass(Canvas*canvas,bool moving,float course_to,float my_course){
    int cx=100,cy=30,r=14;
    canvas_draw_circle(canvas,cx,cy,r);
    if(!moving){canvas_set_font(canvas,FontSecondary);canvas_draw_str(canvas,97,cy-r-1,"N");draw_arrow(canvas,cx,cy,r-2,course_to);}
    else{float rel=wrap180(course_to-my_course);draw_arrow(canvas,cx,cy,r-2,rel);}
}
static void draw_bottom_crumbs(Canvas*canvas,float dist_to_next,float remaining,bool is_final){
    const int Y=63,X_LEFT=8,X_MID=63,X_RIGHT=118;char buf[24];
    draw_teardrop(canvas,X_LEFT,Y);draw_flag_filled(canvas,X_RIGHT,Y);
    if(!is_final){
        draw_flag_empty(canvas,X_MID,Y);
        canvas_draw_line(canvas,X_LEFT+6,Y,X_MID-1,Y);canvas_draw_line(canvas,X_MID+7,Y,X_RIGHT-1,Y);
        fmt_dist(buf,sizeof(buf),dist_to_next);canvas_set_font(canvas,FontSecondary);
        canvas_draw_str_aligned(canvas,(X_LEFT+6+X_MID-1)/2,Y-2,AlignCenter,AlignBottom,buf);
        fmt_dist(buf,sizeof(buf),remaining);
        canvas_draw_str_aligned(canvas,(X_MID+7+X_RIGHT-1)/2,Y-2,AlignCenter,AlignBottom,buf);
    }else{
        canvas_draw_line(canvas,X_LEFT+6,Y,X_RIGHT-1,Y);
        fmt_dist(buf,sizeof(buf),dist_to_next);canvas_set_font(canvas,FontSecondary);
        canvas_draw_str_aligned(canvas,(X_LEFT+6+X_RIGHT-1)/2,Y-2,AlignCenter,AlignBottom,buf);
    }
}
static void draw_bottom_nav(Canvas*canvas,float dist_to_poi){
    const int Y=63,X_LEFT=8,X_RIGHT=118;char buf[24];
    draw_teardrop(canvas,X_LEFT,Y);draw_pin(canvas,X_RIGHT,Y);
    canvas_draw_line(canvas,X_LEFT+6,Y,X_RIGHT-1,Y);
    fmt_dist(buf,sizeof(buf),dist_to_poi);canvas_set_font(canvas,FontSecondary);
    canvas_draw_str_aligned(canvas,(X_LEFT+6+X_RIGHT-1)/2,Y-2,AlignCenter,AlignBottom,buf);
}
static void draw_name_input(Canvas*canvas,AppState*app,const char*title){
    canvas_set_font(canvas,FontSecondary);canvas_draw_str(canvas,2,10,title);canvas_draw_line(canvas,0,12,128,12);
    if(!app->name_input_has_fix){
        canvas_set_font(canvas,FontPrimary);canvas_draw_str_aligned(canvas,64,36,AlignCenter,AlignBottom,"No GPS fix!");
        canvas_set_font(canvas,FontSecondary);canvas_draw_str_aligned(canvas,64,52,AlignCenter,AlignBottom,"Back=cancel");
    }else{
        char display[NAME_MAX+3];char cur_ch[2];int cx;
        canvas_set_font(canvas,FontPrimary);
        snprintf(display,sizeof(display),"%-12s",app->name_input_buf);
        canvas_draw_str(canvas,2,30,display);
        if(app->name_input_active){
            cur_ch[0]=app->name_input_buf[app->name_input_cursor];cur_ch[1]=0;
            if(cur_ch[0]==0||cur_ch[0]==' ')cur_ch[0]='_';
            {char pfx[NAME_MAX+1];strncpy(pfx,display,app->name_input_cursor);pfx[app->name_input_cursor]='\0';
            cx=2+canvas_string_width(canvas,pfx);}
            {char ccs[2]={cur_ch[0],'\0'};int cw=canvas_string_width(canvas,ccs);if(cw<1)cw=4;
            canvas_draw_line(canvas,cx,31,cx+cw-1,31);}
            canvas_draw_str(canvas,104,42,cur_ch);
        }else{canvas_set_font(canvas,FontSecondary);canvas_draw_str(canvas,2,42,"Press LT/RT to edit");}
        canvas_set_font(canvas,FontSecondary);
        canvas_draw_str(canvas,2,50,"UP/DN=char LT/RT=pos");
        canvas_draw_str(canvas,2,58,"OK=save  Back=cancel");
    }
}

// ─── draw callback ────────────────────────────────────────────────────────────
static void draw_callback(Canvas*canvas,void*ctx){
    AppState*app=ctx;GpsUart*gu=app->gps_uart;
    furi_mutex_acquire(gu->mutex,FuriWaitForever);
    canvas_clear(canvas);canvas_set_color(canvas,ColorBlack);
    char buf[64];AppView v=gu->view_state;GpsStatus*st=&gu->status;

    if(v==VIEW_CHANGE_BAUDRATE){canvas_set_font(canvas,FontPrimary);
        canvas_draw_str_aligned(canvas,64,28,AlignCenter,AlignBottom,"Baudrate:");
        snprintf(buf,sizeof(buf),"%d",gps_baudrates[current_gps_baudrate]);
        canvas_draw_str_aligned(canvas,64,44,AlignCenter,AlignBottom,buf);furi_mutex_release(gu->mutex);return;}
    if(v==VIEW_CHANGE_DEEPSLEEP){canvas_set_font(canvas,FontPrimary);
        canvas_draw_str_aligned(canvas,64,36,AlignCenter,AlignBottom,gu->deep_sleep_enabled?"Deep sleep ON":"Deep sleep OFF");
        furi_mutex_release(gu->mutex);return;}
    if(v==VIEW_CHANGE_SPEEDUNIT){canvas_set_font(canvas,FontPrimary);
        canvas_draw_str_aligned(canvas,64,28,AlignCenter,AlignBottom,"Speed unit:");
        canvas_draw_str_aligned(canvas,64,44,AlignCenter,AlignBottom,speed_unit_name(gu->speed_units));
        furi_mutex_release(gu->mutex);return;}

    if(app->overlay_ticks>0&&app->overlay_msg[0]){
        canvas_set_font(canvas,FontPrimary);
        canvas_draw_str_aligned(canvas,64,36,AlignCenter,AlignBottom,app->overlay_msg);
        furi_mutex_release(gu->mutex);return;}

    if(v==VIEW_SETTINGS){
        canvas_set_font(canvas,FontSecondary);
        canvas_draw_str(canvas,2,10,"SETTINGS OK=save Bk=exit");
        canvas_draw_line(canvas,0,12,128,12);
        const char*items[5];char si0[24],si1[24],si2[24],si3[24],si4[32];
        snprintf(si0,sizeof(si0),"Speed: %s",speed_unit_name(gu->speed_units));items[0]=si0;
        snprintf(si1,sizeof(si1),"Baud: %d",gps_baudrates[current_gps_baudrate]);items[1]=si1;
        snprintf(si2,sizeof(si2),"Sleep: %s",gu->deep_sleep_enabled?"ON ":"OFF");items[2]=si2;
        snprintf(si3,sizeof(si3),"TZ: UTC%+d",gu->tz_offset);items[3]=si3;
        if(!app->area_active) snprintf(si4,sizeof(si4),"Start area calc");
        else if(app->area_clear_confirm) snprintf(si4,sizeof(si4),"Clear S? OK=yes");
        else snprintf(si4,sizeof(si4),"S=%.0f m2 (%d)",(double)app->area_m2,app->area_poly_count);
        items[4]=si4;
        for(int i=0;i<5;i++){
            int y=20+i*10;
            if(i==app->settings_sel){canvas_draw_box(canvas,0,y-7,128,9);canvas_set_color(canvas,ColorWhite);}
            else canvas_set_color(canvas,ColorBlack);
            canvas_draw_str(canvas,2,y,items[i]);canvas_set_color(canvas,ColorBlack);
        }
        furi_mutex_release(gu->mutex);return;}

    if(v==VIEW_CRUMBS_MENU){
        if(app->crumbs_clear_confirm){
            canvas_set_font(canvas,FontPrimary);canvas_draw_str_aligned(canvas,64,24,AlignCenter,AlignBottom,"Clear ALL crumbs?");
            canvas_set_font(canvas,FontSecondary);canvas_draw_str_aligned(canvas,64,44,AlignCenter,AlignBottom,"OK=yes  Back=no");
        }else{
            canvas_set_font(canvas,FontPrimary);canvas_draw_str_aligned(canvas,64,12,AlignCenter,AlignBottom,"Crumbs menu");
            canvas_draw_line(canvas,0,14,128,14);
            const char*items[]={"Delete current","Clear all","Last Point","Back"};
            for(int i=0;i<4;i++){
                int y=22+i*11;
                if(i==app->crumbs_menu_sel){canvas_draw_box(canvas,0,y-7,128,9);canvas_set_color(canvas,ColorWhite);}
                else canvas_set_color(canvas,ColorBlack);
                canvas_set_font(canvas,FontSecondary);canvas_draw_str_aligned(canvas,64,y,AlignCenter,AlignBottom,items[i]);
                canvas_set_color(canvas,ColorBlack);
            }
        }
        furi_mutex_release(gu->mutex);return;}

    if(v==VIEW_POI_COORD){
        int row=coord_cursor_to_row(app->coord_cursor);int col=coord_cursor_to_col(app->coord_cursor);
        char*active_str=(row==0)?app->coord_lat_str:app->coord_lon_str;
        canvas_set_font(canvas,FontSecondary);canvas_draw_str(canvas,2,10,"Nav coords (LT/RT=move):");
        canvas_set_font(canvas,FontPrimary);canvas_draw_str(canvas,2,26,app->coord_lat_str);canvas_draw_str(canvas,2,40,app->coord_lon_str);
        int base_y=(row==0)?27:41;
        {char pfx[COORD_LAT_LEN+2];strncpy(pfx,active_str,col);pfx[col]='\0';
        int px=2+canvas_string_width(canvas,pfx);char ccs[2]={active_str[col],'\0'};
        int cw=canvas_string_width(canvas,ccs);if(cw<1)cw=4;canvas_draw_line(canvas,px,base_y,px+cw-1,base_y);}
        canvas_set_font(canvas,FontSecondary);
        {char ccs2[2]={active_str[col],'\0'};canvas_draw_str(canvas,58,54,ccs2);}
        canvas_draw_str(canvas,2,54,"UP/DN=digit");canvas_draw_str(canvas,2,60,"OK=confirm  Back=exit");
        furi_mutex_release(gu->mutex);return;}

    if(v==VIEW_NAV_LIST){
        canvas_set_font(canvas,FontSecondary);
        if(app->poi_delete_confirm&&app->poi_delete_idx>=0&&app->poi_delete_idx<app->poi_count){
            canvas_set_font(canvas,FontPrimary);canvas_draw_str_aligned(canvas,64,20,AlignCenter,AlignBottom,"Delete POI?");
            canvas_set_font(canvas,FontSecondary);canvas_draw_str_aligned(canvas,64,34,AlignCenter,AlignBottom,app->poi[app->poi_delete_idx].name);
            canvas_draw_str_aligned(canvas,64,50,AlignCenter,AlignBottom,"OK=yes  Back=no");
        }else if(app->poi_count==0){
            canvas_draw_str_aligned(canvas,64,20,AlignCenter,AlignBottom,"No POI saved");
            canvas_draw_str_aligned(canvas,64,36,AlignCenter,AlignBottom,"Long DN from main=add");
        }else{
            canvas_draw_str(canvas,0,8,"OK=sel LLT=del LRT=ren");canvas_draw_line(canvas,0,10,128,10);
            int start=app->poi_list_scroll;
            for(int i=0;i<4&&start+i<app->poi_count;i++){
                int y=20+i*11;PoiPoint*p=&app->poi[start+i];bool sel=(start+i==app->poi_list_scroll);
                if(sel){canvas_draw_box(canvas,0,y-7,128,9);canvas_set_color(canvas,ColorWhite);}
                bool co=(app->last_lat!=0.0f||app->last_lon!=0.0f);
                if(p->icon==POI_ICON_HOME)canvas_draw_str(canvas,1,y,"H");else canvas_draw_str(canvas,1,y,"*");
                char dist[16];
                if(app->had_fix&&co){float d=haversine_m(app->last_lat,app->last_lon,p->lat,p->lon);fmt_dist(dist,sizeof(dist),d);}
                else strncpy(dist,"---",sizeof(dist));
                snprintf(buf,sizeof(buf),"%-10s%s",p->name,dist);canvas_draw_str(canvas,10,y,buf);
                canvas_set_color(canvas,ColorBlack);
            }
        }
        furi_mutex_release(gu->mutex);return;}

    if(v==VIEW_NAV){
        bool hf=(st->valid||st->fix_quality>0);bool co=(st->latitude!=0.0f||st->longitude!=0.0f);
        if(hf&&co){app->last_lat=st->latitude;app->last_lon=st->longitude;app->last_course=app->smoothed_course;app->had_fix=true;}
        bool has_target=(app->poi_target>=0&&app->poi_target<app->poi_count);
        if(!has_target){
            canvas_set_font(canvas,FontPrimary);canvas_draw_str_aligned(canvas,64,24,AlignCenter,AlignBottom,"No POI target");
            canvas_set_font(canvas,FontSecondary);canvas_draw_str_aligned(canvas,64,38,AlignCenter,AlignBottom,"Long UP = select POI");
            canvas_draw_str_aligned(canvas,64,50,AlignCenter,AlignBottom,"Long DN = enter coords");
            furi_mutex_release(gu->mutex);return;}
        PoiPoint*dest=&app->poi[app->poi_target];
        bool use_ok=app->had_fix&&(app->last_lat!=0.0f||app->last_lon!=0.0f);
        float dist=use_ok?haversine_m(app->last_lat,app->last_lon,dest->lat,dest->lon):0;
        float course=use_ok?bearing_deg(app->last_lat,app->last_lon,dest->lat,dest->lon):0;
        bool moving=(st->speed_knots*KNOTS_TO_MS>=MOVING_SPEED_MS);
        canvas_set_font(canvas,FontSecondary);
        if(dest->icon==POI_ICON_HOME)snprintf(buf,sizeof(buf),"[H] %s",dest->name);
        else snprintf(buf,sizeof(buf),"[*] %s",dest->name);
        canvas_draw_str(canvas,0,8,buf);
        if(!hf&&app->had_fix)canvas_draw_str(canvas,0,17,"(last fix)");
        if(app->had_fix)draw_compass(canvas,moving,course,app->last_course);
        if(app->had_fix){snprintf(buf,sizeof(buf),"Crs:%.0f\xb0",(double)course);canvas_set_font(canvas,FontSecondary);canvas_draw_str(canvas,0,25,buf);}
        fmt_speed(buf,sizeof(buf),st->speed_knots,gu->speed_units);canvas_set_font(canvas,FontSecondary);canvas_draw_str(canvas,0,34,buf);
        {char tmp2[24];if(!app->nav_show_eta){if(use_ok){fmt_dist(tmp2,sizeof(tmp2),dist);snprintf(buf,sizeof(buf),"->%s",tmp2);}else strncpy(buf,"->---",sizeof(buf));}
        else{float spd=avg_speed_ms(app);if(spd>0.1f&&use_ok){float eta=dist/spd;if(eta<3600)snprintf(buf,sizeof(buf),"ETA %.0fmin",(double)(eta/60.0f));else snprintf(buf,sizeof(buf),"ETA %.1fh",(double)(eta/3600.0f));}else snprintf(buf,sizeof(buf),"ETA ---");}
        canvas_draw_str(canvas,0,44,buf);}
        if(use_ok)draw_bottom_nav(canvas,dist);
        furi_mutex_release(gu->mutex);return;}

    if(v==VIEW_CRUMBS){
        int n=app->point_count,tgt=app->crumbs_target;
        if(n==0){canvas_set_font(canvas,FontPrimary);canvas_draw_str_aligned(canvas,64,32,AlignCenter,AlignBottom,"No track points");
            canvas_set_font(canvas,FontSecondary);canvas_draw_str_aligned(canvas,64,48,AlignCenter,AlignBottom,"GPS recording...");
            furi_mutex_release(gu->mutex);return;}
        bool hf=(st->valid||st->fix_quality>0);bool co=(st->latitude!=0.0f||st->longitude!=0.0f);
        if(hf&&co){app->last_lat=st->latitude;app->last_lon=st->longitude;app->last_course=app->smoothed_course;app->had_fix=true;}
        bool moving=(st->speed_knots*KNOTS_TO_MS>=MOVING_SPEED_MS);
        TrackPoint*dest=&app->points[tgt];
        bool use_ok=app->had_fix&&(app->last_lat!=0.0f||app->last_lon!=0.0f);
        float dist_next=use_ok?haversine_m(app->last_lat,app->last_lon,dest->lat,dest->lon):0;
        float course_to=use_ok?bearing_deg(app->last_lat,app->last_lon,dest->lat,dest->lon):0;
        bool is_final=(tgt==0);int crumbs_total=(n>0)?n-1:0;
        canvas_set_font(canvas,FontSecondary);
        if(dest->time[0])snprintf(buf,sizeof(buf),"CRB %d/%d [%s]",tgt,crumbs_total,dest->time);
        else snprintf(buf,sizeof(buf),"CRB %d/%d",tgt,crumbs_total);
        canvas_draw_str(canvas,0,8,buf);
        if(!hf&&app->had_fix)canvas_draw_str(canvas,0,17,"(last fix)");
        if(app->had_fix)draw_compass(canvas,moving,course_to,app->last_course);
        if(app->had_fix){snprintf(buf,sizeof(buf),"Crs:%.0f\xb0",(double)course_to);canvas_set_font(canvas,FontSecondary);canvas_draw_str(canvas,0,25,buf);}
        fmt_speed(buf,sizeof(buf),st->speed_knots,gu->speed_units);canvas_set_font(canvas,FontSecondary);canvas_draw_str(canvas,0,34,buf);
        if(!app->crumbs_show_total){if(use_ok){char tmp3[24];float tr=dist_next+app->remaining_track_dist;fmt_dist(tmp3,sizeof(tmp3),tr);snprintf(buf,sizeof(buf),"Tot:%s",tmp3);canvas_draw_str(canvas,0,44,buf);}}
        if(use_ok)draw_bottom_crumbs(canvas,dist_next,app->remaining_track_dist,is_final);
        furi_mutex_release(gu->mutex);return;}

    if(v==VIEW_NAME_INPUT){draw_name_input(canvas,app,"Save POI");furi_mutex_release(gu->mutex);return;}
    if(v==VIEW_POI_RENAME){draw_name_input(canvas,app,"Rename POI");furi_mutex_release(gu->mutex);return;}

    // NORMAL
    canvas_set_font(canvas,FontPrimary);
    canvas_draw_str_aligned(canvas,32, 8, AlignCenter,AlignBottom,"Latitude");
    canvas_draw_str_aligned(canvas,96, 8, AlignCenter,AlignBottom,"Longitude");
    canvas_draw_str_aligned(canvas,21, 30,AlignCenter,AlignBottom,"Course");
    canvas_draw_str_aligned(canvas,64, 30,AlignCenter,AlignBottom,"Speed");
    canvas_draw_str_aligned(canvas,107,30,AlignCenter,AlignBottom,"Altitude");
    canvas_draw_str_aligned(canvas,32, 52,AlignCenter,AlignBottom,"Satellites");
    canvas_draw_str_aligned(canvas,96, 52,AlignCenter,AlignBottom,"Last Fix");
    canvas_set_font(canvas,FontSecondary);
    snprintf(buf,sizeof(buf),"%f",(double)st->latitude);canvas_draw_str_aligned(canvas,32, 18,AlignCenter,AlignBottom,buf);
    snprintf(buf,sizeof(buf),"%f",(double)st->longitude);canvas_draw_str_aligned(canvas,96, 18,AlignCenter,AlignBottom,buf);
    snprintf(buf,sizeof(buf),"%.1f",(double)st->course);canvas_draw_str_aligned(canvas,21, 40,AlignCenter,AlignBottom,buf);
    fmt_speed(buf,sizeof(buf),st->speed_knots,gu->speed_units);canvas_draw_str_aligned(canvas,64, 40,AlignCenter,AlignBottom,buf);
    snprintf(buf,sizeof(buf),"%.1f%c",(double)st->altitude,st->altitude_units);canvas_draw_str_aligned(canvas,107,40,AlignCenter,AlignBottom,buf);
    snprintf(buf,sizeof(buf),"%d",st->satellites_tracked);canvas_draw_str_aligned(canvas,32, 62,AlignCenter,AlignBottom,buf);
    {int th=(st->time_hours+gu->tz_offset+24)%24;snprintf(buf,sizeof(buf),"%02d:%02d:%02d",th,st->time_minutes,st->time_seconds);
    canvas_draw_str_aligned(canvas,96,62,AlignCenter,AlignBottom,buf);}
    furi_mutex_release(gu->mutex);
}

static void input_callback(InputEvent*ie,void*ctx){AppEvent ev={.type=EvtTypeInput,.input=*ie};furi_message_queue_put((FuriMessageQueue*)ctx,&ev,FuriWaitForever);}
static void tick_callback(void*ctx){AppEvent ev={.type=EvtTypeTick};furi_message_queue_put((FuriMessageQueue*)ctx,&ev,0);}

// ─── entry point ──────────────────────────────────────────────────────────────
int32_t gps_app(void*p){
    UNUSED(p);
    Expansion*expansion=furi_record_open(RECORD_EXPANSION);expansion_disable(expansion);
    FuriMessageQueue*event_queue=furi_message_queue_alloc(8,sizeof(AppEvent));
    AppState*app=malloc(sizeof(AppState));memset(app,0,sizeof(AppState));
    app->poi_target=-1;app->poi_delete_idx=-1;app->poi_rename_idx=-1;

    GpsUart*gu=gps_uart_enable();app->gps_uart=gu;

    Storage*storage=furi_record_open(RECORD_STORAGE);
    ensure_dir(storage);
    settings_load(gu,storage);
    gps_uart_deinit_thread(gu);gu->baudrate=(uint32_t)gps_baudrates[current_gps_baudrate];gps_uart_init_thread(gu);

    gu->mutex=furi_mutex_alloc(FuriMutexTypeNormal);
    if(!gu->mutex){free(app);gps_uart_disable(gu);furi_record_close(RECORD_STORAGE);expansion_enable(expansion);furi_record_close(RECORD_EXPANSION);furi_message_queue_free(event_queue);return 255;}

    uint8_t attempts=0;
    while(!furi_hal_power_is_otg_enabled()&&attempts++<5){furi_hal_power_enable_otg();furi_delay_ms(10);}
    furi_delay_ms(200);

    track_load(app,storage);poi_load(app,storage);state_load(app,storage);
    if(app->poi_target>=app->poi_count)app->poi_target=-1;
    if(app->point_count>0){
        if(app->crumbs_target>=app->point_count||app->crumbs_target<0)crumbs_target_reset(app);
        else nav_update_dists(app);
    }

    ViewPort*vp=view_port_alloc();
    view_port_draw_callback_set(vp,draw_callback,app);
    view_port_input_callback_set(vp,input_callback,event_queue);
    Gui*gui=furi_record_open(RECORD_GUI);gui_add_view_port(gui,vp,GuiLayerFullscreen);

    FuriTimer*timer=furi_timer_alloc(tick_callback,FuriTimerTypePeriodic,event_queue);
    furi_timer_start(timer,furi_ms_to_ticks(1000));
    bool processing=true;

    while(processing){
        AppEvent event;
        if(furi_message_queue_get(event_queue,&event,100)!=FuriStatusOk)continue;

        if(event.type==EvtTypeTick){
            app->tick_count++;
            furi_mutex_acquire(gu->mutex,FuriWaitForever);
            float spd=gu->status.speed_knots*KNOTS_TO_MS;
            bool has_fix=(gu->status.valid||gu->status.fix_quality>0);
            float lat=gu->status.latitude,lon=gu->status.longitude;
            int hh=gu->status.time_hours,mm=gu->status.time_minutes;
            float raw_course=gu->status.course;
            bool has_coords=(lat!=0.0f||lon!=0.0f);
            int sats=gu->status.satellites_tracked;
            AppView cur_view=gu->view_state;
            furi_mutex_release(gu->mutex);

            app->speed_buf[app->speed_buf_pos%SPEED_BUF_SIZE]=spd;
            app->speed_buf_pos++;if(app->speed_buf_filled<SPEED_BUF_SIZE)app->speed_buf_filled++;
            app->course_buf[app->course_buf_pos%COURSE_SMOOTH]=raw_course;
            app->course_buf_pos++;if(app->course_buf_filled<COURSE_SMOOTH)app->course_buf_filled++;
            app->smoothed_course=course_smooth_calc(app);

            // крошки
            if(cur_view!=VIEW_CRUMBS&&has_fix&&has_coords){
                if(!app->first_fix_done){app->first_fix_done=true;app->first_fix_tick=app->tick_count;}
                furi_mutex_acquire(gu->mutex,FuriWaitForever);int tz=gu->tz_offset;furi_mutex_release(gu->mutex);
                int th=(hh+tz+24)%24;bool should_write=false;
                if(!app->first_point_written&&(app->tick_count-app->first_fix_tick)>=60)
                    {app->first_point_written=true;app->last_save_tick=app->tick_count;should_write=true;}
                else if(app->first_point_written&&(app->tick_count-app->last_save_tick)>=600)
                    {app->last_save_tick=app->tick_count;should_write=true;}
                if(should_write){
                    bool too_close=false;
                    if(app->point_count>0){float d=haversine_m(lat,lon,app->points[app->point_count-1].lat,app->points[app->point_count-1].lon);if(d<MIN_CRUMB_DIST_M)too_close=true;}
                    if(!too_close){track_write_point(lat,lon,th,mm,storage);track_load(app,storage);}
                }
            }

            // площадь
            if(app->area_active&&has_fix&&has_coords)area_tick(app,lat,lon);

            // overlay
            if(app->overlay_ticks>0){
                app->overlay_ticks--;
                if(app->overlay_ticks==0){
                    app->overlay_msg[0]='\0';
                    furi_mutex_acquire(gu->mutex,FuriWaitForever);
                    AppView cv=gu->view_state;
                    if(cv==VIEW_CHANGE_SPEEDUNIT||cv==VIEW_CHANGE_BAUDRATE||cv==VIEW_CHANGE_DEEPSLEEP)
                        gu->view_state=VIEW_SETTINGS;
                    furi_mutex_release(gu->mutex);
                }
            }

            furi_mutex_acquire(gu->mutex,FuriWaitForever);
            if(gu->view_state==VIEW_CRUMBS)crumbs_check_arrive(app);
            furi_mutex_release(gu->mutex);

            // LED статус на основном экране
            if(cur_view==VIEW_NORMAL&&!furi_hal_power_is_charging()){
                NotificationApp*nled=gu->notifications;
                if(!has_fix&&!has_coords&&sats==0)
                    notification_message_block(nled,&sequence_blink_red_10);
                else if(!has_fix||!has_coords)
                    {notification_message_block(nled,&sequence_blink_blue_10);notification_message_block(nled,&sequence_blink_blue_10);}
                else if(app->tick_count%3==0)
                    notification_message_block(nled,&sequence_blink_green_10);
            } else if(cur_view!=VIEW_NORMAL){
                float tgt_lat=0,tgt_lon=0;
                if(cur_view==VIEW_CRUMBS&&app->point_count>0)
                    {tgt_lat=app->points[app->crumbs_target].lat;tgt_lon=app->points[app->crumbs_target].lon;}
                else if(cur_view==VIEW_NAV&&app->poi_target>=0&&app->poi_target<app->poi_count)
                    {tgt_lat=app->poi[app->poi_target].lat;tgt_lon=app->poi[app->poi_target].lon;}
                led_update(app,cur_view,app->last_lat,app->last_lon,spd,tgt_lat,tgt_lon,app->smoothed_course);
            }

            view_port_update(vp);continue;
        }

        if(event.type!=EvtTypeInput)continue;
        InputEvent*ie=&event.input;
        furi_mutex_acquire(gu->mutex,FuriWaitForever);
        AppView cur=gu->view_state;

        // кольцо
        if((cur==VIEW_NORMAL||cur==VIEW_NAV||cur==VIEW_CRUMBS)&&ie->type==InputTypeLong&&(ie->key==InputKeyRight||ie->key==InputKeyLeft)){
            AppView next=(ie->key==InputKeyRight)?ring_right(cur):ring_left(cur);
            float my_lat=gu->status.latitude,my_lon=gu->status.longitude;
            bool going_to_nav=(next==VIEW_NAV),going_to_crumbs=(next==VIEW_CRUMBS);
            gu->view_state=next;furi_mutex_release(gu->mutex);
            if(going_to_crumbs){
                track_load(app,storage);
                if(app->crumbs_reset_on_next_entry){crumbs_target_reset(app);app->crumbs_reset_on_next_entry=false;}
                else{if(app->crumbs_target>=app->point_count&&app->point_count>0)crumbs_target_reset(app);else nav_update_dists(app);}
            }else if(going_to_nav){
                if(cur==VIEW_CRUMBS)app->crumbs_reset_on_next_entry=true;
                char saved_poi_name[NAME_MAX+1]="";
                if(app->poi_target>=0&&app->poi_target<app->poi_count)strncpy(saved_poi_name,app->poi[app->poi_target].name,NAME_MAX);
                poi_load(app,storage);if(app->poi_count>0)poi_sort_by_dist(app,my_lat,my_lon);
                if(saved_poi_name[0]){app->poi_target=-1;for(int i=0;i<app->poi_count;i++){if(strcmp(app->poi[i].name,saved_poi_name)==0){app->poi_target=i;break;}}}
                state_save(app,storage);
            }
            if(app->led_on){led_off(gu->notifications);app->led_on=false;}app->led_white_ticks=0;
            view_port_update(vp);continue;}

        // Back
        if(ie->type==InputTypeShort&&ie->key==InputKeyBack){
            if(cur==VIEW_POI_COORD){gu->view_state=VIEW_NAV;furi_mutex_release(gu->mutex);view_port_update(vp);continue;}
            if(cur==VIEW_POI_RENAME){gu->view_state=VIEW_NAV_LIST;furi_mutex_release(gu->mutex);view_port_update(vp);continue;}
            if(cur==VIEW_NAME_INPUT){gu->view_state=VIEW_NORMAL;furi_mutex_release(gu->mutex);view_port_update(vp);continue;}
            if(cur==VIEW_NAV){app->crumbs_reset_on_next_entry=true;if(app->led_on){led_off(gu->notifications);app->led_on=false;}app->led_white_ticks=0;state_save(app,storage);gu->view_state=VIEW_NORMAL;furi_mutex_release(gu->mutex);view_port_update(vp);continue;}
            if(cur==VIEW_CRUMBS||cur==VIEW_NAV_LIST){if(app->led_on){led_off(gu->notifications);app->led_on=false;}app->led_white_ticks=0;state_save(app,storage);gu->view_state=VIEW_NORMAL;furi_mutex_release(gu->mutex);view_port_update(vp);continue;}
        }

        // CRUMBS MENU
        if(cur==VIEW_CRUMBS_MENU){
            if(app->crumbs_clear_confirm){
                if(ie->type==InputTypeShort&&ie->key==InputKeyOk){
                    app->crumbs_clear_confirm=false;furi_mutex_release(gu->mutex);
                    storage_simply_remove(storage,TRACK_FILE);
                    app->point_count=0;app->crumbs_target=0;app->total_track_dist=0;app->passed_track_dist=0;app->remaining_track_dist=0;
                    furi_mutex_acquire(gu->mutex,FuriWaitForever);
                    bool hf2=(gu->status.valid||gu->status.fix_quality>0);float la2=gu->status.latitude,lo2=gu->status.longitude;
                    int hh2=gu->status.time_hours,mm2=gu->status.time_minutes;int tz2=gu->tz_offset;furi_mutex_release(gu->mutex);
                    if(hf2&&(la2!=0.0f||lo2!=0.0f)){int th2=(hh2+tz2+24)%24;track_write_point(la2,lo2,th2,mm2,storage);track_load(app,storage);crumbs_target_reset(app);}
                    app->crumbs_reset_on_next_entry=true;strncpy(app->overlay_msg,"Track cleared!",sizeof(app->overlay_msg)-1);app->overlay_ticks=3;
                    furi_mutex_acquire(gu->mutex,FuriWaitForever);gu->view_state=VIEW_CRUMBS;furi_mutex_release(gu->mutex);view_port_update(vp);continue;
                }else if(ie->type==InputTypeShort&&ie->key==InputKeyBack){app->crumbs_clear_confirm=false;furi_mutex_release(gu->mutex);view_port_update(vp);continue;}
                furi_mutex_release(gu->mutex);view_port_update(vp);continue;}
            if(ie->type==InputTypeShort&&ie->key==InputKeyBack){gu->view_state=VIEW_CRUMBS;furi_mutex_release(gu->mutex);view_port_update(vp);continue;}
            if((ie->type==InputTypeShort||ie->type==InputTypeRepeat)&&ie->key==InputKeyUp){if(app->crumbs_menu_sel>0)app->crumbs_menu_sel--;}
            else if((ie->type==InputTypeShort||ie->type==InputTypeRepeat)&&ie->key==InputKeyDown){if(app->crumbs_menu_sel<3)app->crumbs_menu_sel++;}
            else if(ie->type==InputTypeShort&&ie->key==InputKeyOk){
                if(app->crumbs_menu_sel==3){gu->view_state=VIEW_CRUMBS;}
                else if(app->crumbs_menu_sel==1){app->crumbs_clear_confirm=true;}
                else if(app->crumbs_menu_sel==2){crumbs_target_reset(app);state_save(app,storage);gu->view_state=VIEW_CRUMBS;}
                else{int idx=app->crumbs_target;furi_mutex_release(gu->mutex);
                    track_delete_point(app,idx,storage);track_load(app,storage);
                    furi_mutex_acquire(gu->mutex,FuriWaitForever);
                    if(app->point_count==0){
                        bool hf3=(gu->status.valid||gu->status.fix_quality>0);float la3=gu->status.latitude,lo3=gu->status.longitude;
                        int hh3=gu->status.time_hours,mm3=gu->status.time_minutes;int tz3=gu->tz_offset;furi_mutex_release(gu->mutex);
                        if(hf3&&(la3!=0.0f||lo3!=0.0f)){int th3=(hh3+tz3+24)%24;track_write_point(la3,lo3,th3,mm3,storage);track_load(app,storage);}
                        app->crumbs_reset_on_next_entry=true;furi_mutex_acquire(gu->mutex,FuriWaitForever);
                    }else if(app->crumbs_target>=app->point_count)app->crumbs_target=app->point_count-1;
                    nav_update_dists(app);strncpy(app->overlay_msg,"Point deleted!",sizeof(app->overlay_msg)-1);app->overlay_ticks=3;gu->view_state=VIEW_CRUMBS;}
            }
            furi_mutex_release(gu->mutex);view_port_update(vp);continue;}

        // CRUMBS
        if(cur==VIEW_CRUMBS){
            if(ie->type==InputTypeLong&&ie->key==InputKeyOk){app->crumbs_menu_sel=0;gu->view_state=VIEW_CRUMBS_MENU;furi_mutex_release(gu->mutex);view_port_update(vp);continue;}
            if(ie->type==InputTypeShort||ie->type==InputTypeRepeat){
                if(ie->key==InputKeyUp&&app->crumbs_target<app->point_count-1){app->crumbs_target++;nav_update_dists(app);state_save(app,storage);led_crumb_switch(app);}
                else if(ie->key==InputKeyDown&&app->crumbs_target>0){app->crumbs_target--;nav_update_dists(app);state_save(app,storage);led_crumb_switch(app);}
                else if(ie->key==InputKeyOk)app->crumbs_show_total=!app->crumbs_show_total;
            }
            furi_mutex_release(gu->mutex);view_port_update(vp);continue;}

        // POI COORD
        if(cur==VIEW_POI_COORD){
            if(ie->type==InputTypeShort&&ie->key==InputKeyOk){
                float lat_v=coord_lat_to_float(app->coord_lat_str);float lon_v=coord_lon_to_float(app->coord_lon_str);
                char name[16];snprintf(name,sizeof(name),"NAV-%02d",app->poi_count+1);
                furi_mutex_release(gu->mutex);poi_add(app,storage,name,POI_ICON_PIN,lat_v,lon_v);
                poi_load(app,storage);for(int i=0;i<app->poi_count;i++){if(strcmp(app->poi[i].name,name)==0){app->poi_target=i;break;}}
                state_save(app,storage);furi_mutex_acquire(gu->mutex,FuriWaitForever);gu->view_state=VIEW_NAV;furi_mutex_release(gu->mutex);view_port_update(vp);continue;}
            if(ie->type==InputTypeShort&&ie->key==InputKeyLeft)app->coord_cursor=coord_next_cursor(app->coord_cursor,-1);
            else if(ie->type==InputTypeShort&&ie->key==InputKeyRight)app->coord_cursor=coord_next_cursor(app->coord_cursor,1);
            else if(ie->type==InputTypeShort||ie->type==InputTypeRepeat){if(ie->key==InputKeyUp)coord_change_char(app,1);else if(ie->key==InputKeyDown)coord_change_char(app,-1);}
            furi_mutex_release(gu->mutex);view_port_update(vp);continue;}

        // NAV
        if(cur==VIEW_NAV){
            if(ie->type==InputTypeShort&&ie->key==InputKeyOk){app->nav_show_eta=!app->nav_show_eta;furi_mutex_release(gu->mutex);view_port_update(vp);continue;}
            if(ie->type==InputTypeLong&&ie->key==InputKeyUp){
                poi_load(app,storage);float la=gu->status.latitude,lo=gu->status.longitude;furi_mutex_release(gu->mutex);
                if(app->poi_count>0)poi_sort_by_dist(app,la,lo);
                app->poi_list_scroll=0;app->poi_delete_confirm=false;
                furi_mutex_acquire(gu->mutex,FuriWaitForever);gu->view_state=VIEW_NAV_LIST;furi_mutex_release(gu->mutex);view_port_update(vp);continue;}
            if(ie->type==InputTypeLong&&ie->key==InputKeyDown){
                float la=gu->status.latitude,lo=gu->status.longitude;coord_from_float(la,lo,app->coord_lat_str,app->coord_lon_str);
                app->coord_cursor=0;gu->view_state=VIEW_POI_COORD;furi_mutex_release(gu->mutex);view_port_update(vp);continue;}
            furi_mutex_release(gu->mutex);view_port_update(vp);continue;}

        // POI LIST
        if(cur==VIEW_NAV_LIST){
            if(app->poi_delete_confirm){
                if(ie->type==InputTypeShort&&ie->key==InputKeyOk){
                    int idx=app->poi_delete_idx;app->poi_delete_confirm=false;app->poi_delete_idx=-1;
                    for(int i=idx;i<app->poi_count-1;i++)app->poi[i]=app->poi[i+1];
                    app->poi_count--;
                    if(app->poi_target>=app->poi_count)app->poi_target=-1;
                    if(app->poi_list_scroll>=app->poi_count&&app->poi_list_scroll>0)app->poi_list_scroll--;
                    furi_mutex_release(gu->mutex);poi_save(app,storage);state_save(app,storage);
                    strncpy(app->overlay_msg,"POI deleted!",sizeof(app->overlay_msg)-1);app->overlay_ticks=2;view_port_update(vp);continue;
                }else if(ie->type==InputTypeShort&&ie->key==InputKeyBack){app->poi_delete_confirm=false;app->poi_delete_idx=-1;furi_mutex_release(gu->mutex);view_port_update(vp);continue;}
                furi_mutex_release(gu->mutex);view_port_update(vp);continue;}
            if(ie->type==InputTypeShort||ie->type==InputTypeRepeat){
                if(ie->key==InputKeyUp&&app->poi_list_scroll>0)app->poi_list_scroll--;
                else if(ie->key==InputKeyDown&&app->poi_list_scroll<app->poi_count-1)app->poi_list_scroll++;
                else if(ie->key==InputKeyOk&&app->poi_count>0){app->poi_target=app->poi_list_scroll;state_save(app,storage);gu->view_state=VIEW_NAV;}}
            if(ie->type==InputTypeLong&&ie->key==InputKeyLeft&&app->poi_count>0){app->poi_delete_idx=app->poi_list_scroll;app->poi_delete_confirm=true;}
            if(ie->type==InputTypeLong&&ie->key==InputKeyRight&&app->poi_count>0){app->poi_rename_idx=app->poi_list_scroll;name_input_start(app,gu,app->poi[app->poi_list_scroll].name,app->poi[app->poi_list_scroll].icon,true);}
            furi_mutex_release(gu->mutex);view_port_update(vp);continue;}

        // NAME INPUT
        if(cur==VIEW_NAME_INPUT||cur==VIEW_POI_RENAME){
            if(ie->type==InputTypeShort&&ie->key==InputKeyOk){furi_mutex_release(gu->mutex);name_input_save(app,storage,gu);view_port_update(vp);continue;}
            if(ie->type==InputTypeShort&&ie->key==InputKeyBack){gu->view_state=(app->name_input_is_rename)?VIEW_NAV_LIST:VIEW_NORMAL;furi_mutex_release(gu->mutex);view_port_update(vp);continue;}
            if(!app->name_input_active){
                if(ie->type==InputTypeShort&&(ie->key==InputKeyLeft||ie->key==InputKeyRight)){
                    app->name_input_active=true;
                    if(ie->key==InputKeyRight&&app->name_input_cursor<NAME_MAX-1)app->name_input_cursor++;
                    else if(ie->key==InputKeyLeft&&app->name_input_cursor>0)app->name_input_cursor--;
                }
                furi_mutex_release(gu->mutex);view_port_update(vp);continue;}
            if((ie->type==InputTypeShort||ie->type==InputTypeRepeat)&&ie->key==InputKeyLeft){if(app->name_input_cursor>0)app->name_input_cursor--;}
            else if((ie->type==InputTypeShort||ie->type==InputTypeRepeat)&&ie->key==InputKeyRight){if(app->name_input_cursor<NAME_MAX-1)app->name_input_cursor++;}
            else if((ie->type==InputTypeShort||ie->type==InputTypeRepeat)&&ie->key==InputKeyUp){char cc=app->name_input_buf[app->name_input_cursor];if(cc==0||cc==' ')cc=' ';app->name_input_buf[app->name_input_cursor]=name_next_char(cc,1);}
            else if((ie->type==InputTypeShort||ie->type==InputTypeRepeat)&&ie->key==InputKeyDown){char cc=app->name_input_buf[app->name_input_cursor];if(cc==0||cc==' ')cc=' ';app->name_input_buf[app->name_input_cursor]=name_next_char(cc,-1);}
            furi_mutex_release(gu->mutex);view_port_update(vp);continue;}

        // SETTINGS
        if(cur==VIEW_SETTINGS){
            if(ie->type==InputTypeShort&&ie->key==InputKeyBack){
                gu->speed_units=app->settings_snap_speed;gu->tz_offset=app->settings_snap_tz;
                gu->deep_sleep_enabled=app->settings_snap_deepsleep;current_gps_baudrate=app->settings_snap_baudrate_idx;
                gu->baudrate=(uint32_t)gps_baudrates[current_gps_baudrate];
                app->settings_dirty=false;gu->view_state=VIEW_NORMAL;furi_mutex_release(gu->mutex);view_port_update(vp);continue;}
            if((ie->type==InputTypeShort||ie->type==InputTypeRepeat)&&ie->key==InputKeyUp){if(app->settings_sel>0)app->settings_sel--;furi_mutex_release(gu->mutex);view_port_update(vp);continue;}
            if((ie->type==InputTypeShort||ie->type==InputTypeRepeat)&&ie->key==InputKeyDown){if(app->settings_sel<4)app->settings_sel++;furi_mutex_release(gu->mutex);view_port_update(vp);continue;}
            if((ie->type==InputTypeShort||ie->type==InputTypeRepeat)&&(ie->key==InputKeyLeft||ie->key==InputKeyRight)){
                int dir=(ie->key==InputKeyRight)?1:-1;
                switch(app->settings_sel){
                case 0:gu->speed_units=(SpeedUnit)((gu->speed_units+dir+4)%4);app->settings_dirty=true;break;
                case 1:if(dir>0)current_gps_baudrate=(current_gps_baudrate+1)%6;else current_gps_baudrate=(current_gps_baudrate+5)%6;app->settings_dirty=true;break;
                case 2:gu->deep_sleep_enabled=!gu->deep_sleep_enabled;app->settings_dirty=true;break;
                case 3:gu->tz_offset+=dir;if(gu->tz_offset>14)gu->tz_offset=-12;else if(gu->tz_offset<-12)gu->tz_offset=14;app->settings_dirty=true;break;
                case 4: // area: ←/→ предлагает сброс
                    if(!app->area_active){}
                    else if(app->area_clear_confirm)app->area_clear_confirm=false;
                    else app->area_clear_confirm=true;
                    break;
                default:break;}
                furi_mutex_release(gu->mutex);view_port_update(vp);continue;}
            if(ie->type==InputTypeShort&&ie->key==InputKeyOk){
                // area OK
                if(app->settings_sel==4){
                    if(!app->area_active){
                        app->area_active=true;app->area_poly_count=0;app->area_m2=0;
                        app->area_smooth_pos=0;app->area_smooth_count=0;app->area_clear_confirm=false;
                        float clat=gu->status.latitude,clon=gu->status.longitude;
                        if((clat==0.0f&&clon==0.0f)&&app->had_fix){clat=app->last_lat;clon=app->last_lon;}
                        furi_mutex_release(gu->mutex);
                        if(clat!=0.0f||clon!=0.0f)area_add_vertex(app,clat,clon);
                        view_port_update(vp);continue;
                    }else if(app->area_clear_confirm){
                        furi_mutex_release(gu->mutex);
                        area_save_file(app,storage);
                        app->area_active=false;app->area_poly_count=0;app->area_m2=0;app->area_clear_confirm=false;
                        view_port_update(vp);continue;
                    }
                    furi_mutex_release(gu->mutex);view_port_update(vp);continue;
                }
                // settings OK — сохраняем
                if(app->settings_dirty){
                    if(gu->baudrate!=(uint32_t)gps_baudrates[current_gps_baudrate]){
                        gps_uart_deinit_thread(gu);gu->baudrate=(uint32_t)gps_baudrates[current_gps_baudrate];gps_uart_init_thread(gu);}
                    if(gu->deep_sleep_enabled)furi_hal_serial_tx(gu->serial_handle,(uint8_t*)"$PMTK161,0*28\r\n",strlen("$PMTK161,0*28\r\n"));
                    else{uint8_t wake=0xFF;furi_hal_serial_tx(gu->serial_handle,&wake,1);furi_delay_ms(100);}
                    furi_hal_serial_tx(gu->serial_handle,(uint8_t*)"$PMTK220,1000*1F\r\n",strlen("$PMTK220,1000*1F\r\n"));
                    furi_hal_serial_tx(gu->serial_handle,(uint8_t*)"$PMTK314,0,1,0,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0*28\r\n",strlen("$PMTK314,0,1,0,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0*28\r\n"));
                    app->settings_dirty=false;
                }
                furi_mutex_release(gu->mutex);settings_save(gu,storage);
                furi_mutex_acquire(gu->mutex,FuriWaitForever);
                gu->view_state=VIEW_NORMAL;furi_mutex_release(gu->mutex);view_port_update(vp);continue;}
            furi_mutex_release(gu->mutex);view_port_update(vp);continue;}

        // NORMAL
        if(ie->type==InputTypeShort&&ie->key==InputKeyBack){processing=false;furi_mutex_release(gu->mutex);continue;}
        if(ie->type==InputTypeShort&&ie->key==InputKeyOk){
            app->settings_snap_speed=gu->speed_units;app->settings_snap_tz=gu->tz_offset;
            app->settings_snap_baudrate_idx=current_gps_baudrate;app->settings_snap_deepsleep=gu->deep_sleep_enabled;
            app->settings_dirty=false;app->settings_sel=0;
            gu->view_state=VIEW_SETTINGS;furi_mutex_release(gu->mutex);view_port_update(vp);continue;}
        if(ie->type==InputTypeLong&&ie->key==InputKeyUp){name_input_start(app,gu,"HOME",POI_ICON_HOME,false);furi_mutex_release(gu->mutex);view_port_update(vp);continue;}
        if(ie->type==InputTypeLong&&ie->key==InputKeyDown){char def[16];snprintf(def,sizeof(def),"POI-%02d",app->poi_count+1);name_input_start(app,gu,def,POI_ICON_PIN,false);furi_mutex_release(gu->mutex);view_port_update(vp);continue;}
        furi_mutex_release(gu->mutex);view_port_update(vp);
    }

    state_save(app,storage);settings_save(gu,storage);
    if(app->area_active&&app->area_poly_count>0)area_save_file(app,storage);
    if(app->led_on)led_off(gu->notifications);
    furi_timer_stop(timer);furi_timer_free(timer);
    view_port_enabled_set(vp,false);gui_remove_view_port(gui,vp);view_port_free(vp);
    furi_record_close(RECORD_GUI);furi_mutex_free(gu->mutex);gps_uart_disable(gu);free(app);
    furi_message_queue_free(event_queue);furi_record_close(RECORD_STORAGE);
    expansion_enable(expansion);furi_record_close(RECORD_EXPANSION);
    return 0;
}
