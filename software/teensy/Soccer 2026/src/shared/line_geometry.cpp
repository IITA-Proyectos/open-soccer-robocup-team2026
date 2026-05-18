#include "line_geometry.h"
#include <cmath>
namespace iitasoccer {
static int16_t to_cd(float deg){
    while (deg > 180.0f)   deg -= 360.0f;
    while (deg <= -180.0f) deg += 360.0f;
    return (int16_t)lroundf(deg*100.0f);
}
float lg_sensor_angle_deg(int i, int n){
    if(n<=0)return 0.0f;
    float d = (360.0f*(float)i)/(float)n;     // 0=frente, sentido horario
    while (d > 180.0f) d -= 360.0f;
    return d;
}
GeomResult lg_compute(const bool* white, const float* ang, int n){
    GeomResult g{}; g.line_present=false; g.corner=false;
    g.line_angle_centideg=LSV2_NA_I16; g.escape_angle_centideg=LSV2_NA_I16;
    double sx=0.0, sy=0.0; int cnt=0;
    for(int i=0;i<n;++i){ if(white[i]){ ++cnt;
        double r=ang[i]*M_PI/180.0; sx+=cos(r); sy+=sin(r); } }
    g.sensors_on_line=(uint8_t)(cnt>255?255:cnt);
    if(cnt==0) return g;
    g.line_present=true;
    double a = atan2(sy,sx)*180.0/M_PI;
    g.line_angle_centideg = to_cd((float)a);
    g.escape_angle_centideg = to_cd((float)a + 180.0f);

    // CORNER: ≥2 clusters de blancos separados, con separación angular ~90°±35°.
    // Cluster = corrida de sensores blancos contiguos (anillo cerrado).
    {
        int first=-1; for(int i=0;i<n;++i){ if(!white[i]){first=i;break;} }
        int clusters=0; double cmean[8]; int cn=0; // hasta 8 clusters
        if(first<0){ /* todos blancos: una sola superficie, no corner */ }
        else {
            bool inrun=false; double sxx=0,syy=0;
            for(int k=0;k<n;++k){ int i=(first+k)%n;
                if(white[i]){ if(!inrun){inrun=true;sxx=0;syy=0;}
                    double r=ang[i]*M_PI/180.0; sxx+=cos(r); syy+=sin(r); }
                else if(inrun){ inrun=false; if(cn<8){
                    cmean[cn++]=atan2(syy,sxx)*180.0/M_PI; clusters++; } }
            }
            if(inrun && cn<8){ cmean[cn++]=atan2(syy,sxx)*180.0/M_PI; clusters++; }
        }
        if(clusters>=2){
            for(int i=0;i<cn && !g.corner;++i) for(int j=i+1;j<cn;++j){
                double d=fabs(cmean[i]-cmean[j]); if(d>180.0)d=360.0-d;
                if(d>=55.0 && d<=125.0){ g.corner=true; break; }
            }
        }
    }

    return g;
}
}  // namespace iitasoccer
