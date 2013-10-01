#define PORT_ACT_LEFT       1
#define PORT_ACT_RIGHT      2
#define PORT_ACT_FWD        3
#define PORT_ACT_BACK       4
#define PORT_ACT_LOOK_UP    5
#define PORT_ACT_LOOK_DOWN  6
#define PORT_ACT_MOVE_LEFT  7
#define PORT_ACT_MOVE_RIGHT 8
#define PORT_ACT_STRAFE     9
#define PORT_ACT_SPEED      10
#define PORT_ACT_USE        11
#define PORT_ACT_JUMP       12
#define PORT_ACT_ATTACK     13
#define PORT_ACT_UP         14
#define PORT_ACT_DOWN       15

#define PORT_ACT_NEXT_WEP   16
#define PORT_ACT_PREV_WEP   17

#define PORT_ACT_MAP         30
#define PORT_ACT_MAP_UP      31
#define PORT_ACT_MAP_DOWN    32
#define PORT_ACT_MAP_LEFT    33
#define PORT_ACT_MAP_RIGHT   34
#define PORT_ACT_MAP_ZOOM_IN 35
#define PORT_ACT_MAP_ZOOM_OUT 36

#define PORT_ACT_ZOOM_IN 37

#define LOOK_MODE_MOUSE    0
#define LOOK_MODE_ABSOLUTE 1
#define LOOK_MODE_JOYSTICK 2



#ifdef __cplusplus
extern "C"
{
#endif
int PortableKeyEvent(int state, int code ,int unitcode);
void PortableAction(int state, int action);

void PortableMove(float fwd, float strafe);
void PortableMoveFwd(float fwd);
void PortableMoveSide(float strafe);
void PortableLookPitch(int mode, float pitch);
void PortableLookYaw(int mode, float pitch);
void PortableCommand(const char * cmd);

void PortableMouse(float dx,float dy);

void PortableMouseAbs(float x,float y);

void PortableInit(int argc,const char ** argv);
void PortableFrame(void);

int PortableInMenu(void);
int PortableShowKeyboard(void);
int PortableInAutomap(void);


#ifdef __cplusplus
}
#endif
