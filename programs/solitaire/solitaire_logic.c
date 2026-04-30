// #include <windows.h>
// #include <stdlib.h>
// #include <time.h>
// #include "solitaire.h"

// #define X_MARGIN        10
// #define Y_MARGIN        7
// #define X_SPACING       79
// #define Y_TOP_ROW       Y_MARGIN
// #define Y_TABLEAU       (Y_MARGIN + CARD_H + 16)
// #define FACE_DOWN_OFFSET 18
// #define FACE_UP_OFFSET   28
// #define CARD_W           71
// #define CARD_H           96

// /* Game state */
// static CARD deck[52];
// static CARD stock[52];      int stock_top = 0;
// static CARD waste[52];      int waste_top = 0;
// static CARD foundation[4][52]; int found_top[4];
// static CARD tableau[7][52]; int tab_top[7];

// /* Drag state */
// static BOOL  dragging = FALSE;
// static CARD  drag_cards[52];
// static int   drag_count = 0;
// static int   drag_from_type;   /* SRC_STOCK, SRC_WASTE, SRC_TAB, SRC_FOUND */
// static int   drag_from_idx;
// static int   drag_mouse_x, drag_mouse_y;
// static int   drag_card_ox, drag_card_oy; /* offset from card origin to mouse */

// /* Scoring & timer */
// static int   score = 0;
// static DWORD start_tick = 0;
// static UINT  timer_id = 0;

// #define SRC_STOCK  0
// #define SRC_WASTE  1
// #define SRC_TAB    2
// #define SRC_FOUND  3

// static HWND g_hwnd;

// /* ---- helpers ---- */
// static int card_suit(CARD c)  { return c & 3; }
// static int card_face(CARD c)  { return c >> 2; }
// static BOOL is_red(CARD c)    { int s=card_suit(c); return s==CARD_SUIT_DIAMONDS||s==CARD_SUIT_HEARTS; }

// static void shuffle(CARD *arr, int n)
// {
//     int i, j; CARD t;
//     for(i = n-1; i > 0; i--) {
//         j = rand() % (i+1);
//         t=arr[i]; arr[i]=arr[j]; arr[j]=t;
//     }
// }

// void InitGame(void)
// {
//     int i, col, row;
//     CARD d[52];

//     score = 0;
//     start_tick = GetTickCount();
//     dragging = FALSE;

//     for(i=0;i<52;i++) d[i]=(CARD)i;
//     shuffle(d,52);

//     /* deal tableau */
//     i=0;
//     for(col=0;col<7;col++) {
//         tab_top[col]=col+1;
//         for(row=0;row<=col;row++) {
//             tableau[col][row] = d[i] | (row==col ? CARD_FACEUP : 0);
//             i++;
//         }
//     }
//     /* rest to stock, face down */
//     stock_top=0;
//     while(i<52) stock[stock_top++]=d[i++];
//     waste_top=0;
//     for(i=0;i<4;i++) found_top[i]=0;
// }

// /* ---- card position helpers ---- */
// static int tab_col_x(int col) { return X_MARGIN + col*X_SPACING; }

// static int tab_card_y(int col, int row)
// {
//     int y=Y_TABLEAU, j;
//     for(j=0;j<row;j++)
//         y += (tableau[col][j]&CARD_FACEUP) ? FACE_UP_OFFSET : FACE_DOWN_OFFSET;
//     return y;
// }

// static int stock_x(void)      { return X_MARGIN; }
// static int waste_x(void)      { return X_MARGIN + X_SPACING; }
// static int found_x(int i)     { return X_MARGIN + (3+i)*X_SPACING; }
// static int top_y(void)        { return Y_TOP_ROW; }

// /* ---- hit testing ---- */
// /* Returns TRUE if (mx,my) is inside card at (cx,cy) */
// static BOOL hit(int mx,int my,int cx,int cy)
// {
//     return mx>=cx && mx<cx+CARD_W && my>=cy && my<cy+CARD_H;
// }

// /* Find which tableau card was clicked, returns row index or -1 */
// static int hit_tab_card(int col, int mx, int my)
// {
//     int row, y=Y_TABLEAU, ny, j;
//     int n=tab_top[col];
//     if(n==0) {
//         if(hit(mx,my,tab_col_x(col),Y_TABLEAU)) return 0; /* empty slot */
//         return -1;
//     }
//     for(j=0;j<n;j++) {
//         ny = y + ((tableau[col][j]&CARD_FACEUP)?FACE_UP_OFFSET:FACE_DOWN_OFFSET);
//         if(j==n-1) ny=y+CARD_H; /* last card full height */
//         if(my>=y && my<ny && mx>=tab_col_x(col) && mx<tab_col_x(col)+CARD_W)
//             return j;
//         y += (tableau[col][j]&CARD_FACEUP)?FACE_UP_OFFSET:FACE_DOWN_OFFSET;
//     }
//     /* check last card */
//     if(hit(mx,my,tab_col_x(col),y)) return n-1;
//     return -1;
// }

// /* ---- can_drop logic ---- */
// static BOOL can_drop_tab(CARD c, int col)
// {
//     CARD top;
//     if(!(c&CARD_FACEUP)) return FALSE;
//     if(tab_top[col]==0) return card_face(c)==12; /* King on empty */
//     top = tableau[col][tab_top[col]-1];
//     if(!(top&CARD_FACEUP)) return FALSE;
//     return card_face(top)==card_face(c)+1 && is_red(top)!=is_red(c);
// }

// static BOOL can_drop_found(CARD c, int f)
// {
//     if(!(c&CARD_FACEUP)) return FALSE;
//     if(found_top[f]==0) return card_face(c)==0; /* Ace */
//     CARD top=foundation[f][found_top[f]-1];
//     return card_suit(c)==card_suit(top) && card_face(c)==card_face(top)+1;
// }

// /* ---- drawing ---- */
// void DrawBoard(HDC hdc, int width, int height)
// {
//     int i, col, row, y;
//     WCHAR buf[64];
//     RECT rcStatus, rcScore, rcTime;
//     HBRUSH hbr;
//     HPEN hpen, hOldPen;
//     HFONT hfont, hOldFont;
//     DWORD elapsed;

//     /* Status bar */
//     rcStatus.left=0; rcStatus.top=height-STATUS_BAR_HEIGHT;
//     rcStatus.right=width; rcStatus.bottom=height;
//     hbr=CreateSolidBrush(GetSysColor(COLOR_BTNFACE));
//     FillRect(hdc,&rcStatus,hbr); DeleteObject(hbr);
//     hpen=CreatePen(PS_SOLID,1,GetSysColor(COLOR_BTNSHADOW));
//     hOldPen=(HPEN)SelectObject(hdc,hpen);
//     MoveToEx(hdc,0,height-STATUS_BAR_HEIGHT,NULL);
//     LineTo(hdc,width,height-STATUS_BAR_HEIGHT);
//     SelectObject(hdc,hOldPen); DeleteObject(hpen);

//     hfont=CreateFontW(13,0,0,0,FW_NORMAL,0,0,0,ANSI_CHARSET,
//         OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,DEFAULT_QUALITY,
//         DEFAULT_PITCH|FF_SWISS,L"MS Sans Serif");
//     hOldFont=(HFONT)SelectObject(hdc,hfont);
//     SetBkMode(hdc,TRANSPARENT);

//     elapsed = (GetTickCount()-start_tick)/1000;
//     wsprintfW(buf,L"Score: %d",score);
//     rcScore=rcStatus; rcScore.left+=4;
//     DrawTextW(hdc,buf,-1,&rcScore,DT_SINGLELINE|DT_VCENTER|DT_LEFT);

//     wsprintfW(buf,L"Time: %d",(int)elapsed);
//     rcTime=rcStatus; rcTime.right-=4;
//     DrawTextW(hdc,buf,-1,&rcTime,DT_SINGLELINE|DT_VCENTER|DT_RIGHT);

//     SelectObject(hdc,hOldFont); DeleteObject(hfont);

//     /* Stock */
//     if(stock_top>0) {
//         if(stock_top>1) cdtDraw(hdc,stock_x()-2,top_y()-2,CARD_BACK_WEAVE1,MODE_FACEDOWN,SOL_BG_COLOR);
//         cdtDraw(hdc,stock_x(),top_y(),CARD_BACK_WEAVE1,MODE_FACEDOWN,SOL_BG_COLOR);
//     } else {
//         cdtDraw(hdc,stock_x(),top_y(),CARD_FREE_MASK,MODE_GHOST,SOL_BG_COLOR);
//     }

//     /* Waste */
//     if(waste_top>0)
//         cdtDraw(hdc,waste_x(),top_y(),waste[waste_top-1]&~CARD_FACEUP,MODE_FACEUP,SOL_BG_COLOR);
//     else
//         cdtDraw(hdc,waste_x(),top_y(),CARD_FREE_MASK,MODE_GHOST,SOL_BG_COLOR);

//     /* Foundations */
//     for(i=0;i<4;i++) {
//         int fx=found_x(i);
//         if(found_top[i]==0)
//             cdtDraw(hdc,fx,top_y(),CARD_FREE_MASK,MODE_GHOST,SOL_BG_COLOR);
//         else
//             cdtDraw(hdc,fx,top_y(),foundation[i][found_top[i]-1]&~CARD_FACEUP,MODE_FACEUP,SOL_BG_COLOR);
//     }

//     /* Tableau */
//     for(col=0;col<7;col++) {
//         int cx=tab_col_x(col);
//         if(tab_top[col]==0) {
//             cdtDraw(hdc,cx,Y_TABLEAU,CARD_FREE_MASK,MODE_GHOST,SOL_BG_COLOR);
//             continue;
//         }
//         y=Y_TABLEAU;
//         for(row=0;row<tab_top[col];row++) {
//             CARD c=tableau[col][row];
//             /* skip dragged cards */
//             if(dragging && drag_from_type==SRC_TAB && drag_from_idx==col && row>=tab_top[col]-drag_count)
//                 break;
//             if(c&CARD_FACEUP)
//                 cdtDraw(hdc,cx,y,c&~CARD_FACEUP,MODE_FACEUP,SOL_BG_COLOR);
//             else
//                 cdtDraw(hdc,cx,y,CARD_BACK_WEAVE1,MODE_FACEDOWN,SOL_BG_COLOR);
//             y+=(c&CARD_FACEUP)?FACE_UP_OFFSET:FACE_DOWN_OFFSET;
//         }
//     }

//     /* Drag cards */
//     if(dragging) {
//         y=drag_mouse_y - drag_card_oy;
//         int x=drag_mouse_x - drag_card_ox;
//         for(i=0;i<drag_count;i++) {
//             cdtDraw(hdc,x,y+i*FACE_UP_OFFSET,drag_cards[i]&~CARD_FACEUP,MODE_FACEUP,SOL_BG_COLOR);
//         }
//     }
// }

// /* ---- mouse handling ---- */
// void OnLButtonDown(HWND hwnd, int mx, int my)
// {
//     int col, row, i;

//     /* Stock click */
//     if(hit(mx,my,stock_x(),top_y())) {
//         if(stock_top>0) {
//             waste[waste_top++] = stock[--stock_top] | CARD_FACEUP;
//         } else {
//             /* reset */
//             while(waste_top>0) stock[stock_top++]=waste[--waste_top]&~CARD_FACEUP;
//             score = score>100 ? score-100 : 0;
//         }
//         InvalidateRect(hwnd,NULL,FALSE);
//         return;
//     }

//     /* Waste click — start drag */
//     if(waste_top>0 && hit(mx,my,waste_x(),top_y())) {
//         drag_cards[0]=waste[waste_top-1]|CARD_FACEUP;
//         drag_count=1;
//         drag_from_type=SRC_WASTE;
//         drag_from_idx=0;
//         drag_card_ox=mx-waste_x();
//         drag_card_oy=my-top_y();
//         dragging=TRUE;
//         drag_mouse_x=mx; drag_mouse_y=my;
//         SetCapture(hwnd);
//         InvalidateRect(hwnd,NULL,FALSE);
//         return;
//     }

//     /* Foundation click */
//     for(i=0;i<4;i++) {
//         if(found_top[i]>0 && hit(mx,my,found_x(i),top_y())) {
//             drag_cards[0]=foundation[i][found_top[i]-1]|CARD_FACEUP;
//             drag_count=1;
//             drag_from_type=SRC_FOUND;
//             drag_from_idx=i;
//             drag_card_ox=mx-found_x(i);
//             drag_card_oy=my-top_y();
//             dragging=TRUE;
//             drag_mouse_x=mx; drag_mouse_y=my;
//             SetCapture(hwnd);
//             InvalidateRect(hwnd,NULL,FALSE);
//             return;
//         }
//     }

//     /* Tableau click */
//     for(col=0;col<7;col++) {
//         int cx=tab_col_x(col);
//         int n=tab_top[col];
//         if(n==0) continue;

//         /* find topmost visible card hit */
//         int card_y=Y_TABLEAU;
//         int hit_row=-1;
//         for(row=0;row<n;row++) {
//             int next_y=card_y+((tableau[col][row]&CARD_FACEUP)?FACE_UP_OFFSET:FACE_DOWN_OFFSET);
//             if(row==n-1) next_y=card_y+CARD_H;
//             if(mx>=cx && mx<cx+CARD_W && my>=card_y && my<next_y)
//                 hit_row=row;
//             card_y+=((tableau[col][row]&CARD_FACEUP)?FACE_UP_OFFSET:FACE_DOWN_OFFSET);
//         }

//         if(hit_row<0) continue;

//         /* flip face-down top card */
//         if(!(tableau[col][hit_row]&CARD_FACEUP) && hit_row==n-1) {
//             tableau[col][hit_row]|=CARD_FACEUP;
//             score+=5;
//             InvalidateRect(hwnd,NULL,FALSE);
//             return;
//         }

//         /* drag face-up card(s) */
//         if(tableau[col][hit_row]&CARD_FACEUP) {
//             drag_count=n-hit_row;
//             for(i=0;i<drag_count;i++)
//                 drag_cards[i]=tableau[col][hit_row+i];
//             drag_from_type=SRC_TAB;
//             drag_from_idx=col;
//             /* card_y at hit_row */
//             int hy=Y_TABLEAU;
//             for(i=0;i<hit_row;i++)
//                 hy+=((tableau[col][i]&CARD_FACEUP)?FACE_UP_OFFSET:FACE_DOWN_OFFSET);
//             drag_card_ox=mx-cx;
//             drag_card_oy=my-hy;
//             dragging=TRUE;
//             drag_mouse_x=mx; drag_mouse_y=my;
//             SetCapture(hwnd);
//             InvalidateRect(hwnd,NULL,FALSE);
//             return;
//         }
//     }
// }

// void OnMouseMove(HWND hwnd, int mx, int my)
// {
//     if(!dragging) return;
//     drag_mouse_x=mx; drag_mouse_y=my;
//     InvalidateRect(hwnd,NULL,FALSE);
// }

// void OnLButtonUp(HWND hwnd, int mx, int my)
// {
//     int i, col;
//     BOOL dropped=FALSE;

//     if(!dragging) return;
//     ReleaseCapture();
//     dragging=FALSE;

//     CARD top_card=drag_cards[0];

//     /* Try foundation */
//     for(i=0;i<4;i++) {
//         if(drag_count==1 && hit(mx,my,found_x(i),top_y()) && can_drop_found(top_card,i)) {
//             foundation[i][found_top[i]++]=top_card;
//             /* remove from source */
//             if(drag_from_type==SRC_WASTE) waste_top--;
//             else if(drag_from_type==SRC_TAB) {
//                 tab_top[drag_from_idx]--;
//                 /* flip new top */
//                 int n=tab_top[drag_from_idx];
//                 if(n>0 && !(tableau[drag_from_idx][n-1]&CARD_FACEUP)) {
//                     tableau[drag_from_idx][n-1]|=CARD_FACEUP;
//                     score+=5;
//                 }
//             } else if(drag_from_type==SRC_FOUND) found_top[drag_from_idx]--;
//             score+=10;
//             dropped=TRUE;
//             break;
//         }
//     }

//     /* Try tableau */
//     if(!dropped) {
//         for(col=0;col<7;col++) {
//             int cx=tab_col_x(col);
//             int ty=Y_TABLEAU;
//             int n=tab_top[col];
//             /* hit area: empty slot or on top of stack */
//             int bot_y= n==0 ? ty+CARD_H : ty;
//             for(i=0;i<n;i++) bot_y+=((tableau[col][i]&CARD_FACEUP)?FACE_UP_OFFSET:FACE_DOWN_OFFSET);
//             if(n>0) bot_y+=CARD_H-(FACE_UP_OFFSET); /* approximate */

//             if(mx>=cx && mx<cx+CARD_W && my>=ty && my<bot_y+CARD_H) {
//                 if(can_drop_tab(top_card,col)) {
//                     for(i=0;i<drag_count;i++)
//                         tableau[col][tab_top[col]++]=drag_cards[i];
//                     /* remove from source */
//                     if(drag_from_type==SRC_WASTE) waste_top--;
//                     else if(drag_from_type==SRC_TAB) {
//                         tab_top[drag_from_idx]-=drag_count;
//                         int n2=tab_top[drag_from_idx];
//                         if(n2>0 && !(tableau[drag_from_idx][n2-1]&CARD_FACEUP)) {
//                             tableau[drag_from_idx][n2-1]|=CARD_FACEUP;
//                             score+=5;
//                         }
//                     } else if(drag_from_type==SRC_FOUND) found_top[drag_from_idx]--;
//                     if(drag_from_type==SRC_WASTE) score+=5;
//                     dropped=TRUE;
//                     break;
//                 }
//             }
//         }
//     }

//     InvalidateRect(hwnd,NULL,FALSE);
// }

// void OnTimer(HWND hwnd)
// {
//     /* Redraw status bar only */
//     RECT rc; GetClientRect(hwnd,&rc);
//     rc.top=rc.bottom-STATUS_BAR_HEIGHT;
//     InvalidateRect(hwnd,&rc,FALSE);
// }