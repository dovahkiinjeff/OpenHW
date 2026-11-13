"""OpenHW's portable, concept-art launcher shell. No game rendering involved."""
from pathlib import Path
import ctypes
from ctypes import wintypes
import json
import os
import queue
import threading
import tkinter as tk
from tkinter import ttk,filedialog,font as tkfont

BG='#081218'; PANEL='#0b1820'; RAISED='#10242d'; LINE='#29444e'
INK='#e2e8e2'; MUTED='#8ca4a9'; GOLD='#e6ae45'; CYAN='#73d4d2'; GREEN='#91c6a0'
BODY=('Rajdhani',-20); SMALL=('Rajdhani',-17); BOLD=('Rajdhani',-21,'bold')
MONO=('Share Tech Mono',-12); HEADING=('Rajdhani',-42,'bold')

def load_fonts(folder):
    if os.name!='nt':return
    add=ctypes.windll.gdi32.AddFontResourceExW
    add.argtypes=[wintypes.LPCWSTR,wintypes.DWORD,ctypes.c_void_p];add.restype=ctypes.c_int
    for name in ('Rajdhani-Regular.ttf','Rajdhani-Bold.ttf','ShareTechMono-Regular.ttf'):
        if not add(str(folder/name),0x10,None):raise RuntimeError('Launcher font is missing: '+name)

def label(parent,text,*,size=BODY,color=INK,bg=PANEL,**kwargs):
    return tk.Label(parent,text=text,font=size,fg=color,bg=bg,bd=0,anchor='w',**kwargs)

class Button(tk.Canvas):
    """Angular, keyboard-operable buttons drawn without native window controls."""
    def __init__(self,parent,text,command,*,primary=False,width=220,height=46,icon='',**kwargs):
        super().__init__(parent,width=width,height=height,bg=kwargs.pop('bg',PANEL),bd=0,highlightthickness=0,takefocus=1,cursor='hand2',**kwargs)
        self.text=text;self.command=command;self.primary=primary;self.icon=icon;self.hover=False;self.focused=False
        self.bind('<Configure>',self.paint);self.bind('<Enter>',lambda e:self.state(True));self.bind('<Leave>',lambda e:self.state(False))
        self.bind('<FocusIn>',lambda e:self.focus(True));self.bind('<FocusOut>',lambda e:self.focus(False))
        self.bind('<ButtonRelease-1>',lambda e:self.invoke());self.bind('<Return>',lambda e:self.invoke());self.bind('<space>',lambda e:self.invoke())
    def state(self,value):self.hover=value;self.paint()
    def focus(self,value):self.focused=value;self.paint()
    def invoke(self):self.focus_set();self.command();return 'break'
    def paint(self,event=None):
        self.delete('all');w=self.winfo_width();h=self.winfo_height();cut=10
        fill=('#f5c769' if self.hover else GOLD) if self.primary else ('#1b3b45' if self.hover else '#10232b')
        edge=CYAN if self.focused else GOLD if self.primary or self.hover else LINE
        self.create_polygon(1,1,w-cut-1,1,w-1,cut+1,w-1,h-1,1,h-1,fill=fill,outline=edge,width=1)
        text_color=BG if self.primary else INK
        self.create_text(20,h//2,text=self.text,fill=text_color,font=BOLD if self.primary else SMALL,anchor='w')
        if self.icon=='play':self.create_polygon(w-37,h//2-9,w-37,h//2+9,w-23,h//2,fill=text_color)
        elif self.icon=='arrow':self.create_line(w-38,h//2,w-23,h//2,w-30,h//2-7,fill=text_color,width=2)
        else:self.create_line(w-27,h//2-4,w-22,h//2,w-27,h//2+4,fill=text_color,width=1)

class Toggle(tk.Canvas):
    def __init__(self,parent,variable,*,numeric=False):
        super().__init__(parent,width=118,height=34,bg=RAISED,highlightthickness=0,takefocus=1,cursor='hand2')
        self.variable=variable;self.numeric=numeric
        variable.trace_add('write',lambda *a:self.paint());self.bind('<Configure>',self.paint)
        self.bind('<Button-1>',lambda e:self.toggle());self.bind('<space>',lambda e:self.toggle())
        self.bind('<FocusIn>',self.paint);self.bind('<FocusOut>',self.paint)
    def toggle(self):
        enabled=self.variable.get() in ('1','TRUE')
        self.variable.set(('0' if enabled else '1') if self.numeric else ('FALSE' if enabled else 'TRUE'));return 'break'
    def paint(self,event=None):
        self.delete('all');on=self.variable.get() in ('1','TRUE')
        self.create_rectangle(1,5,61,28,fill='#294348' if on else '#172b33',outline=CYAN if self.focus_get()==self else LINE)
        x=36 if on else 7;self.create_rectangle(x,9,x+19,24,fill=GOLD if on else MUTED,outline='')
        self.create_text(77,17,text='ON' if on else 'OFF',fill=INK if on else MUTED,font=MONO,anchor='w')

class Chrome:
    def __init__(self,window,close,*,title='OPENHW / LAUNCH CONTROL',height=62,main=False):
        self.window=window;self.close=close;self.main=main;self.restore=None;self.drag=None;self.resizing=None
        window.overrideredirect(True)
        self.bar=tk.Canvas(window,height=height,bg=BG,highlightthickness=0,cursor='fleur')
        self.bar.pack(fill='x',padx=2,pady=(2,0));self.title=title
        self.bar.bind('<Configure>',self.draw);self.bar.bind('<ButtonPress-1>',self.press)
        self.bar.bind('<B1-Motion>',self.move);self.bar.bind('<ButtonRelease-1>',lambda e:setattr(self,'drag',None))
        if main:self.bar.bind('<Double-Button-1>',lambda e:self.maximize())
        window.protocol('WM_DELETE_WINDOW',close)
        def request_close(event):close();return 'break'
        window.bind('<Alt-F4>',request_close);window.bind('<Escape>',request_close if not main else lambda e:None)
        if main:
            window.bind('<Motion>',self.cursor,add='+');window.bind('<ButtonPress-1>',self.resize_start,add='+')
            window.bind('<B1-Motion>',self.resize_move,add='+');window.bind('<ButtonRelease-1>',lambda e:setattr(self,'resizing',None),add='+')
    def draw(self,event=None):
        c=self.bar;c.delete('all');w=c.winfo_width();h=c.winfo_height()
        c.create_line(0,h-1,w,h-1,fill=LINE)
        c.create_polygon(22,17,38,30,22,43,28,30,fill=GOLD,outline='')
        c.create_line(35,19,49,30,35,41,fill=CYAN,width=1)
        c.create_text(66,h//2,text=self.title,fill=INK,font=('Rajdhani',-24,'bold'),anchor='w')
        if self.main:
            c.create_text(w-210,h//2,text='FLEET OPERATIONS',fill=MUTED,font=MONO,anchor='e',tags='window-hint')
            for center,kind in ((w-133,'min'),(w-83,'max'),(w-33,'close')):
                c.create_rectangle(center-21,12,center+21,h-12,fill='#10232b',outline=LINE,tags=kind)
                if kind=='min':c.create_line(center-7,h//2-3,center,h//2+4,center+7,h//2-3,fill=CYAN,width=2,tags=kind)
                elif kind=='max':
                    for dx,dy in ((-1,-1),(1,-1),(-1,1),(1,1)):
                        c.create_line(center+dx*3,h//2+dy*9,center+dx*9,h//2+dy*9,center+dx*9,h//2+dy*3,fill=CYAN,tags=kind)
                else:
                    c.create_arc(center-9,h//2-9,center+9,h//2+9,start=45,extent=270,outline=GOLD,width=2,style='arc',tags=kind)
                    c.create_line(center,h//2-12,center,h//2-1,fill=GOLD,width=2,tags=kind)
                c.tag_bind(kind,'<Enter>',lambda e,k=kind:self.hover(k,True))
                c.tag_bind(kind,'<Leave>',lambda e,k=kind:self.hover(k,False))
        else:
            c.create_text(w-27,h//2,text='BACK',fill=GOLD,font=MONO,anchor='e',tags='close')
    def hover(self,kind,active):
        self.bar.configure(cursor='hand2' if active else 'fleur')
        if self.main:
            hint={'min':'MINIMIZE','max':'RESTORE WINDOW' if self.restore else 'EXPAND WINDOW','close':'CLOSE LAUNCHER'}
            self.bar.itemconfigure('window-hint',text=hint[kind] if active else 'FLEET OPERATIONS',fill=CYAN if active else MUTED)
    def press(self,event):
        w=self.bar.winfo_width()
        if self.main and event.x>w-158:
            if event.x>w-55:self.close()
            elif event.x>w-107:self.maximize()
            else:self.minimize()
            return 'break'
        if not self.main and event.x>w-80:self.close();return 'break'
        if not self.restore:self.drag=(event.x_root-self.window.winfo_x(),event.y_root-self.window.winfo_y())
    def move(self,event):
        if self.drag:self.window.geometry(f'+{event.x_root-self.drag[0]}+{event.y_root-self.drag[1]}')
    def hwnd(self):
        user=ctypes.windll.user32;user.GetParent.argtypes=[wintypes.HWND];user.GetParent.restype=wintypes.HWND
        return user.GetParent(self.window.winfo_id()) or self.window.winfo_id()
    def taskbar(self):
        if os.name!='nt':return
        user=ctypes.windll.user32;get=user.GetWindowLongPtrW;put=user.SetWindowLongPtrW
        get.argtypes=[wintypes.HWND,ctypes.c_int];get.restype=ctypes.c_ssize_t
        put.argtypes=[wintypes.HWND,ctypes.c_int,ctypes.c_ssize_t];put.restype=ctypes.c_ssize_t
        hwnd=self.hwnd();put(hwnd,-20,(get(hwnd,-20)&~0x80)|0x40000)
        user.SetWindowPos.argtypes=[wintypes.HWND,wintypes.HWND,ctypes.c_int,ctypes.c_int,ctypes.c_int,ctypes.c_int,wintypes.UINT]
        user.SetWindowPos(hwnd,None,0,0,0,0,0x27|0x10)
    def minimize(self):
        if os.name=='nt':
            user=ctypes.windll.user32;user.ShowWindow.argtypes=[wintypes.HWND,ctypes.c_int];user.ShowWindow(self.hwnd(),6)
    def maximize(self):
        if self.restore:self.window.geometry(self.restore);self.restore=None;return
        self.restore=self.window.geometry()
        if os.name=='nt':
            class Info(ctypes.Structure):_fields_=[('size',wintypes.DWORD),('monitor',wintypes.RECT),('work',wintypes.RECT),('flags',wintypes.DWORD)]
            user=ctypes.windll.user32;user.MonitorFromWindow.argtypes=[wintypes.HWND,wintypes.DWORD];user.MonitorFromWindow.restype=wintypes.HANDLE
            data=Info();data.size=ctypes.sizeof(data);user.GetMonitorInfoW.argtypes=[wintypes.HANDLE,ctypes.POINTER(Info)]
            user.GetMonitorInfoW(user.MonitorFromWindow(self.hwnd(),2),ctypes.byref(data));r=data.work
            self.window.geometry(f'{r.right-r.left}x{r.bottom-r.top}+{r.left}+{r.top}')
        else:self.window.geometry(f'{self.window.winfo_screenwidth()}x{self.window.winfo_screenheight()}+0+0')
    def edge(self,event):
        x=event.x_root-self.window.winfo_rootx();y=event.y_root-self.window.winfo_rooty();w=self.window.winfo_width();h=self.window.winfo_height()
        return ('w' if x<7 else 'e' if x>w-7 else '')+('n' if y<7 else 's' if y>h-7 else '')
    def cursor(self,event):
        if self.restore:return
        edges=self.edge(event);cursors={'w':'sb_h_double_arrow','e':'sb_h_double_arrow','n':'sb_v_double_arrow','s':'sb_v_double_arrow','wn':'top_left_corner','en':'top_right_corner','ws':'bottom_left_corner','es':'bottom_right_corner'}
        self.window.configure(cursor=cursors.get(edges,''))
    def resize_start(self,event):
        edges=self.edge(event)
        if edges and not self.restore:
            self.resizing=(edges,event.x_root,event.y_root,self.window.winfo_x(),self.window.winfo_y(),self.window.winfo_width(),self.window.winfo_height());return 'break'
    def resize_move(self,event):
        if not self.resizing:return
        edges,sx,sy,x,y,w,h=self.resizing;dx=event.x_root-sx;dy=event.y_root-sy
        if 'e' in edges:w=max(980,w+dx)
        if 's' in edges:h=max(650,h+dy)
        if 'w' in edges:delta=min(dx,w-980);x+=delta;w-=delta
        if 'n' in edges:delta=min(dy,h-650);y+=delta;h-=delta
        self.window.geometry(f'{w}x{h}+{x}+{y}');return 'break'

class App:
    def __init__(self,root,backend,*,preview=False):
        self.root=Path(root).resolve();self.api=backend;self.preview=preview
        backend.updater.detect_version(self.root)
        self.theme=backend.safe(self.root,'Development/Launcher/Theme');load_fonts(self.theme)
        self.window=tk.Tk();self.window.withdraw();self.window.title('OpenHW Launcher');self.window.configure(bg=LINE)
        self.window.tk.call('tk','scaling',1.0)
        for name in ('TkDefaultFont','TkTextFont','TkMenuFont','TkHeadingFont','TkCaptionFont'):
            tkfont.nametofont(name).configure(family='Rajdhani',size=-20)
        self.style=ttk.Style();self.style.theme_use('clam');self.configure_styles()
        self.config=backend.Configuration(self.root);self.mods=backend.read_mods(self.root)
        self.variables={};self.original={};self.widgets={};self.wraps=[];self.choices={}
        self.messages=queue.Queue();self.responses=queue.Queue();self.busy=False;self.tab='play';self.modal=None
        self.version=backend.updater.detect_version(self.root);self.status=tk.StringVar(value='READY FOR DEPLOYMENT')
        self.chrome=Chrome(self.window,self.close,title='OPENHW   /   FLEET COMMAND',main=True)
        self.shell=tk.Frame(self.window,bg=PANEL);self.shell.pack(fill='both',expand=True,padx=2)
        self.rail=tk.Canvas(self.shell,width=204,bg=BG,highlightthickness=0);self.rail.pack(side='left',fill='y')
        self.rail.bind('<Configure>',self.draw_rail);self.rail.bind('<ButtonRelease-1>',self.rail_click)
        self.body=tk.Frame(self.shell,bg=PANEL);self.body.pack(side='left',fill='both',expand=True)
        self.pages={}
        for name in ('play','settings','mods','system'):
            self.pages[name]=tk.Frame(self.body,bg=PANEL)
        self.art=[tk.PhotoImage(file=str(self.theme/f'hangar-{size}.png')) for size in (1000,1440,2200)]
        self.mark=tk.PhotoImage(file=str(self.theme/'mark.png'));self.window.iconphoto(True,self.mark)
        self.make_play();self.make_settings();self.make_mods();self.make_system();self.make_footer();self.show('play');self.status.set('READY FOR DEPLOYMENT / Fleet systems online.')
        self.window.bind_all('<MouseWheel>',self.wheel)
        self.window.bind('<Control-s>',lambda e:self.guarded(self.save))
        width=min(1280,self.window.winfo_screenwidth()-80);height=min(820,self.window.winfo_screenheight()-90)
        width=max(980,width);height=max(650,height)
        x=(self.window.winfo_screenwidth()-width)//2;y=(self.window.winfo_screenheight()-height)//2
        self.window.geometry(f'{width}x{height}+{x}+{y}' if not preview else f'{width}x{height}-32000-32000')
        self.window.minsize(980,650);self.window.update_idletasks()
        if not preview:
            self.window.deiconify();self.window.update_idletasks();self.chrome.taskbar();self.window.after(120,self.chrome.taskbar)
            self.window.after(100,self.poll)
    def configure_styles(self):
        s=self.style;s.configure('.',background=PANEL,foreground=INK,font=BODY,borderwidth=0)
        s.configure('TCombobox',fieldbackground=RAISED,background=RAISED,foreground=INK,arrowcolor=GOLD,bordercolor=LINE,lightcolor=LINE,darkcolor=LINE,padding=8,arrowsize=16)
        s.map('TCombobox',fieldbackground=[('readonly',RAISED)],foreground=[('readonly',INK)],selectbackground=[('readonly',RAISED)],selectforeground=[('readonly',INK)],bordercolor=[('focus',CYAN)])
        self.window.option_add('*TCombobox*Listbox.background',RAISED);self.window.option_add('*TCombobox*Listbox.foreground',INK)
        self.window.option_add('*TCombobox*Listbox.selectBackground','#36505a');self.window.option_add('*TCombobox*Listbox.selectForeground',GOLD)
        self.window.option_add('*TCombobox*Listbox.font',BODY)
        s.configure('Vertical.TScrollbar',background='#3c626a',troughcolor=BG,bordercolor=PANEL,lightcolor='#3c626a',darkcolor='#3c626a',arrowcolor=MUTED,width=9,arrowsize=10)
        s.map('Vertical.TScrollbar',background=[('active',CYAN)])
        s.configure('Treeview',background=RAISED,fieldbackground=RAISED,foreground=INK,font=BODY,rowheight=48,borderwidth=0)
        s.layout('Treeview',[('Treeview.treearea',{'sticky':'nswe'})])
        s.configure('Treeview.Heading',background=BG,foreground=MUTED,font=MONO,relief='flat',padding=12)
        s.map('Treeview',background=[('selected','#314640')],foreground=[('selected','#ffe1a2')])
    def draw_rail(self,event=None):
        c=self.rail;c.delete('all');h=c.winfo_height();c.create_line(203,0,203,h,fill=LINE)
        c.create_text(25,34,text='COMMAND INTERFACE',fill=MUTED,font=MONO,anchor='w')
        names=[('play','DEPLOY'),('settings','CONFIGURATION'),('mods','MOD LOADOUT'),('system','SHIP SYSTEMS')]
        for i,(key,text) in enumerate(names):
            y=80+i*78;active=key==self.tab
            if active:
                c.create_rectangle(0,y-24,203,y+35,fill='#142d35',outline='');c.create_rectangle(0,y-24,3,y+35,fill=GOLD,outline='')
            c.create_text(25,y-4,text=f'0{i+1}',fill=GOLD if active else '#4b6a75',font=MONO,anchor='w')
            c.create_text(25,y+20,text=text,fill=INK if active else MUTED,font=('Rajdhani',-21,'bold'),anchor='w')
        c.create_line(25,h-130,178,h-130,fill=LINE)
        c.create_text(25,h-105,text='LOCAL INSTALLATION',fill=MUTED,font=MONO,anchor='w')
        c.create_text(25,h-79,text='OPENHW '+self.version,fill=GOLD,font=('Rajdhani',-20,'bold'),anchor='w')
        c.create_text(25,h-49,text='PORTABLE / WINDOWS 64',fill='#66858e',font=('Share Tech Mono',-10),anchor='w')
    def rail_click(self,event):
        for i,key in enumerate(('play','settings','mods','system')):
            if 56+i*78<=event.y<=115+i*78:self.show(key);return
    def show(self,name):
        self.tab=name
        for page in self.pages.values():page.pack_forget()
        self.pages[name].pack(fill='both',expand=True);self.draw_rail()
    def page_heading(self,page,title,subtitle):
        header=tk.Frame(page,bg=PANEL);header.pack(fill='x',padx=34,pady=(26,18))
        label(header,title,size=HEADING).pack(anchor='w');label(header,subtitle,size=MONO,color=MUTED).pack(anchor='w',pady=(2,0))
        return header
    def make_play(self):
        self.hero=tk.Canvas(self.pages['play'],bg=PANEL,highlightthickness=0);self.hero.pack(fill='both',expand=True)
        self.hero.bind('<Configure>',self.draw_hero)
        self.play_button=Button(self.hero,'PLAY OPENHW',lambda:self.guarded(self.launch),primary=True,width=352,height=72,icon='play')
        self.diag_button=Button(self.hero,'LAUNCH WITH DIAGNOSTICS',lambda:self.guarded(lambda:self.launch(True)),width=352,height=46)
    def draw_hero(self,event=None):
        c=self.hero;c.delete('scene');w=c.winfo_width();h=c.winfo_height();art=self.art[0 if w<=960 else 1 if w<=1500 else 2]
        c.create_image(w,h//2,image=art,anchor='e',tags='scene')
        c.create_line(36,40,76,40,fill=GOLD,width=2,tags='scene')
        c.create_text(88,40,text='BRIDGE // STANDING BY',fill=CYAN,font=MONO,anchor='w',tags='scene')
        c.create_text(32,118,text='OPENHW',fill=INK,font=('Rajdhani',-94,'bold'),anchor='w',tags='scene')
        c.create_text(38,183,text='T H E   J O U R N E Y   H O M E',fill=GOLD,font=('Rajdhani',-20,'bold'),anchor='w',tags='scene')
        y=max(335,min(h-180,393));self.play_button.place(x=38,y=y,width=352,height=72)
        self.diag_button.place(x=38,y=y+84,width=352,height=46)
        c.create_text(39,h-28,text='BUILD '+self.version+'   /   FLEET SYSTEMS ONLINE',fill=GREEN,font=MONO,anchor='w',tags='scene')
        c.create_text(w-24,h-28,text='ARCHIVE  /  HANGAR STUDY',fill='#b7c9cb',font=('Share Tech Mono',-10),anchor='e',tags='scene')
        c.tag_lower('scene')
    def entry(self,parent,variable,width=24,secret=False):
        frame=tk.Frame(parent,bg=LINE,padx=1,pady=1)
        entry=tk.Entry(frame,textvariable=variable,font=BODY,bg='#10242d',fg=INK,insertbackground=GOLD,relief='flat',bd=0,width=width,
            selectbackground='#3a5b61',selectforeground=INK,highlightthickness=0,show='*' if secret else '')
        entry.pack(fill='both',expand=True,ipadx=10,ipady=7)
        entry.bind('<FocusIn>',lambda e:frame.configure(bg=CYAN));entry.bind('<FocusOut>',lambda e:frame.configure(bg=LINE))
        return frame
    def make_settings(self):
        page=self.pages['settings'];header=self.page_heading(page,'CONFIGURATION','TUNE YOUR EXPERIENCE  /  CHANGES APPLY ON NEXT LAUNCH')
        Button(header,'SAVE SETTINGS',lambda:self.guarded(self.save),width=180,height=42,primary=True).place(relx=1,x=0,y=0,anchor='ne')
        toolbar=tk.Frame(page,bg=PANEL);toolbar.pack(fill='x',padx=34,pady=(0,12))
        self.group=tk.StringVar(value='All settings');self.search=tk.StringVar()
        groups=['All settings']+list(dict.fromkeys(row['group'] for row in self.api.SETTINGS))
        ttk.Combobox(toolbar,textvariable=self.group,values=groups,state='readonly',width=24,font=SMALL).pack(side='left')
        label(toolbar,'FIND',size=MONO,color=MUTED).pack(side='left',padx=(18,12))
        self.entry(toolbar,self.search).pack(side='left',fill='x',expand=True)
        area=tk.Frame(page,bg=PANEL);area.pack(fill='both',expand=True,padx=(34,24),pady=(0,18))
        self.settings_canvas=tk.Canvas(area,bg=PANEL,highlightthickness=0)
        scroll=ttk.Scrollbar(area,orient='vertical',command=self.settings_canvas.yview)
        self.settings_canvas.configure(yscrollcommand=scroll.set);scroll.pack(side='right',fill='y',padx=(12,0));self.settings_canvas.pack(side='left',fill='both',expand=True)
        self.rows=tk.Frame(self.settings_canvas,bg=PANEL);inner=self.settings_canvas.create_window(0,0,window=self.rows,anchor='nw')
        self.rows.bind('<Configure>',lambda e:self.settings_canvas.configure(scrollregion=self.settings_canvas.bbox('all')))
        def resize(event):
            self.settings_canvas.itemconfigure(inner,width=event.width)
            for item in self.wraps:item.configure(wraplength=max(150,event.width-340))
        self.settings_canvas.bind('<Configure>',resize)
        for index,row in enumerate(self.api.SETTINGS):
            key=row['key'];value=self.config.values.get(key,str(row.get('default','0')))
            if row['kind']=='bool':value=('1' if value in ('TRUE','1') else '0') if row.get('numeric_bool') else ('TRUE' if value in ('TRUE','1') else 'FALSE')
            variable=tk.StringVar(value=value);self.variables[key]=variable;self.original[key]=value
            frame=tk.Frame(self.rows,bg=RAISED,padx=16,pady=12);frame.columnconfigure(0,weight=1)
            name=label(frame,row['label'],size=BOLD,bg=RAISED,wraplength=380);name.grid(row=0,column=0,sticky='w');self.wraps.append(name)
            if row.get('help'):
                helptext=label(frame,row['help'],size=SMALL,color=MUTED,bg=RAISED,wraplength=380,justify='left');helptext.grid(row=1,column=0,sticky='w',pady=(3,0));self.wraps.append(helptext)
            if row['kind']=='bool':control=Toggle(frame,variable,numeric=row.get('numeric_bool',False))
            elif row['kind']=='color':
                def hexvalue(v):n=int(v);return '#%02x%02x%02x'%(n&255,n>>8&255,n>>16&255)
                control=Button(frame,hexvalue(value).upper(),lambda:None,width=250,height=40,bg=RAISED)
                def pick(v=variable,c=control):
                    chosen=self.pick_color(hexvalue(v.get()))
                    if chosen:
                        v.set(str(int(chosen[1:3],16)|(int(chosen[3:5],16)<<8)|(int(chosen[5:7],16)<<16)|0xff000000));c.text=chosen.upper();c.paint()
                control.command=lambda f=pick:self.guarded(f)
            elif 'choices' in row or key=='voiceLanguage':
                options=self.api.launcher_languages.available(self.root) if key=='voiceLanguage' else row['choices']
                display=tk.StringVar(value=options.get(value,value))
                control=ttk.Combobox(frame,textvariable=display,values=list(options.values()),state='readonly',width=29,font=SMALL)
                control.bind('<<ComboboxSelected>>',lambda e,v=variable,d=display,o=options:v.set(next(k for k,t in o.items() if t==d.get())))
                self.choices[key]=(control,display,options)
            else:control=self.entry(frame,variable,width=23,secret=key=='PlayerPassword')
            control.grid(row=0,column=1,rowspan=2,sticky='e',padx=(24,0));self.widgets[key]=frame
        self.group.trace_add('write',self.filter_settings);self.search.trace_add('write',self.filter_settings);self.filter_settings()
    def filter_settings(self,*args):
        for row in self.api.SETTINGS:
            frame=self.widgets[row['key']]
            if (self.group.get()=='All settings' or self.group.get()==row['group']) and self.search.get().casefold() in (row['label']+' '+row['key']).casefold():frame.pack(fill='x',pady=(0,3))
            else:frame.pack_forget()
        self.settings_canvas.yview_moveto(0)
    def wheel(self,event):
        if self.tab=='settings' and not self.modal and not isinstance(event.widget,ttk.Combobox):self.settings_canvas.yview_scroll(-int(event.delta/120),'units')
        elif self.tab=='system' and not self.modal:self.system_canvas.yview_scroll(-int(event.delta/120),'units')
    def make_mods(self):
        page=self.pages['mods'];self.page_heading(page,'MOD LOADOUT','ENABLE. ARRANGE. DEPLOY.  /  LATER ENTRIES TAKE PRIORITY')
        bar=tk.Frame(page,bg=PANEL);bar.pack(fill='x',padx=34,pady=(0,16))
        label(bar,'ACTIVE PACKAGES',size=MONO,color=GOLD).pack(side='left')
        self.mod_count=label(bar,'',size=MONO,color=MUTED);self.mod_count.pack(side='right')
        box=tk.Frame(page,bg=LINE,padx=1,pady=1);box.pack(fill='both',expand=True,padx=34)
        self.tree=ttk.Treeview(box,columns=('order','state','name'),show='headings',selectmode='browse')
        for key,title,width in (('order','ORDER',80),('state','STATUS',130),('name','MOD PACKAGE',460)):
            self.tree.heading(key,text=title,anchor='w');self.tree.column(key,width=width,anchor='w',stretch=key=='name')
        scrollbar=ttk.Scrollbar(box,orient='vertical',command=self.tree.yview);self.tree.configure(yscrollcommand=scrollbar.set)
        scrollbar.pack(side='right',fill='y');self.tree.pack(fill='both',expand=True)
        self.tree.tag_configure('on',foreground=GREEN);self.tree.tag_configure('off',foreground=MUTED)
        self.tree.bind('<Double-1>',lambda e:self.guarded(self.toggle_mod));self.tree.bind('<space>',lambda e:self.guarded(self.toggle_mod))
        actions=tk.Frame(page,bg=PANEL);actions.pack(side='bottom',before=box,fill='x',padx=34,pady=(10,12))
        buttons=[]
        for text,action,width in (('ENABLE / DISABLE',self.toggle_mod,174),('MOVE UP',lambda:self.move_mod(-1),120),('MOVE DOWN',lambda:self.move_mod(1),136),('REFRESH',self.refresh_mods,112),('SAVE LOADOUT',self.save_mods,170)):
            buttons.append(Button(actions,text,lambda f=action:self.guarded(f),width=width,height=42,primary=text=='SAVE LOADOUT'))
        def arrange(event):
            columns=5 if event.width>=800 else 3
            for i in range(5):actions.columnconfigure(i,weight=1 if i<columns else 0,uniform='actions' if i<columns else '',minsize=0)
            for i,button in enumerate(buttons):button.grid(row=i//columns,column=i%columns,sticky='ew',padx=(0,8),pady=(0,8))
        actions.bind('<Configure>',arrange)
        label(page,'New packages start disabled. Restart the game after changing your loadout.',size=SMALL,color=MUTED,wraplength=650,justify='left').pack(side='bottom',before=box,anchor='w',padx=34,pady=(12,0))
        self.redraw_mods()
    def redraw_mods(self,selected=None):
        self.tree.delete(*self.tree.get_children())
        for index,row in enumerate(self.mods):self.tree.insert('','end',iid=str(index),values=(f'{index+1:02d}','ACTIVE' if row['enabled'] else 'STANDBY',row['name']),tags=('on' if row['enabled'] else 'off',))
        if selected is not None:self.tree.selection_set(str(selected));self.tree.see(str(selected))
        self.mod_count.configure(text=f'{sum(bool(r["enabled"]) for r in self.mods):02d} ACTIVE  /  {len(self.mods):02d} INSTALLED')
        if not self.mods:self.status.set('MOD LOADOUT EMPTY / Add mod folders to your installation, then refresh.')
    def mod_index(self):return int(self.tree.selection()[0]) if self.tree.selection() else None
    def toggle_mod(self):
        index=self.mod_index()
        if index is not None:self.mods[index]['enabled']=not self.mods[index]['enabled'];self.redraw_mods(index)
    def move_mod(self,direction):
        index=self.mod_index()
        if index is not None and 0<=index+direction<len(self.mods):self.mods[index],self.mods[index+direction]=self.mods[index+direction],self.mods[index];self.redraw_mods(index+direction)
    def refresh_mods(self):self.mods=self.api.read_mods(self.root);self.redraw_mods()
    def save_mods(self):self.api.save_mods(self.root,self.mods);self.status.set('LOADOUT SAVED / Mod priority applies on next launch.')
    def make_system(self):
        page=self.pages['system'];self.page_heading(page,'SHIP SYSTEMS','MAINTENANCE & UPDATES  /  ALL FILES STAY IN YOUR OPENHW FOLDER')
        area=tk.Frame(page,bg=PANEL);area.pack(fill='both',expand=True,padx=(0,16),pady=(0,16))
        self.system_canvas=tk.Canvas(area,bg=PANEL,highlightthickness=0)
        scrollbar=ttk.Scrollbar(area,orient='vertical',command=self.system_canvas.yview);scrollbar.pack(side='right',fill='y')
        self.system_canvas.configure(yscrollcommand=scrollbar.set);self.system_canvas.pack(side='left',fill='both',expand=True)
        contents=tk.Frame(self.system_canvas,bg=PANEL);inner=self.system_canvas.create_window(0,0,window=contents,anchor='nw')
        contents.bind('<Configure>',lambda e:self.system_canvas.configure(scrollregion=self.system_canvas.bbox('all')))
        self.system_canvas.bind('<Configure>',lambda e:self.system_canvas.itemconfigure(inner,width=e.width))
        cards=[('01','UPDATE CHANNEL','Get the latest official patches in the required order.\nDownloads are verified before installation.','CHECK FOR UPDATES',self.update),
               ('02','SHADER LIBRARY','Back up the local shader cache and rebuild on next launch.\nThe next startup will take longer.','REBUILD CACHE',self.cache),
               ('03','VOICE ARCHIVE','Import a complete localized voice bank from your owned\nHomeworld 1 Classic files. Only matching packs are offered.','IMPORT VOICE PACK',self.import_voice)]
        for number,title,description,action,callback in cards:
            card=tk.Frame(contents,bg=RAISED,padx=20,pady=17);card.pack(fill='x',padx=34,pady=(0,12));card.columnconfigure(1,weight=1)
            label(card,number,size=MONO,color=GOLD,bg=RAISED).grid(row=0,column=0,rowspan=2,sticky='n',padx=(0,16),pady=5)
            label(card,title,size=BOLD,bg=RAISED).grid(row=0,column=1,sticky='w')
            text=label(card,description,size=SMALL,color=MUTED,bg=RAISED,justify='left',wraplength=440);text.grid(row=1,column=1,sticky='w',pady=(5,0))
            card.bind('<Configure>',lambda e,t=text:t.configure(wraplength=max(160,e.width-335)))
            Button(card,action,lambda f=callback:self.guarded(f),width=215,height=44,bg=RAISED).grid(row=0,column=2,rowspan=2,padx=(18,0))
        label(contents,'INSTALLATION / '+str(self.root),size=MONO,color=MUTED,wraplength=800,justify='left').pack(anchor='w',padx=34,pady=8)
    def make_footer(self):
        foot=tk.Frame(self.window,bg=BG,height=82);foot.pack(side='bottom',before=self.shell,fill='x',padx=2,pady=(0,2));foot.pack_propagate(False)
        line=tk.Frame(foot,bg=LINE,height=1);line.pack(fill='x')
        label(foot,'SYSTEM STATUS',size=('Share Tech Mono',-10),color=GOLD,bg=BG).place(x=24,y=13)
        status=tk.Label(foot,textvariable=self.status,font=SMALL,fg=INK,bg=BG,anchor='w',justify='left',wraplength=1050)
        status.place(x=24,y=31,relwidth=1,width=-80,height=35)
        self.progress=tk.Canvas(foot,height=3,bg='#17313a',highlightthickness=0);self.progress.pack(side='bottom',fill='x',padx=24,pady=(0,7))
        self.progress.bind('<Configure>',lambda e:self.draw_progress());self.amount=0;self.tick=0
    def draw_progress(self):
        c=self.progress;c.delete('all');w=c.winfo_width()
        if self.busy and self.amount is None:
            x=(self.tick%120)/120*(w+180)-180;c.create_rectangle(x,0,x+180,3,fill=GOLD,outline='')
        elif self.amount:c.create_rectangle(0,0,w*self.amount,3,fill=GOLD,outline='')
    def guarded(self,action):
        try:
            if self.busy:raise self.api.updater.UpdateError('Let the current update finish first.')
            return action()
        except Exception as error:self.dialog('COMMAND INTERRUPTED',str(error))
    def save(self):
        changed={key:variable.get() for key,variable in self.variables.items() if variable.get()!=self.original[key]}
        if changed:self.config.save(changed)
        self.original={key:value.get() for key,value in self.variables.items()};self.status.set('CONFIGURATION SAVED / Changes apply on next launch.')
    def launch(self,diagnostics=False):
        self.save();self.api.save_mods(self.root,self.mods);self.api.start_game(self.root,diagnostics);self.status.set('OPENHW DEPLOYED / '+('Diagnostics enabled.' if diagnostics else 'Have a good flight.'))
    def update(self):
        self.api.updater.stop_if_game_running();self.save();self.api.save_mods(self.root,self.mods);self.busy=True;self.amount=None;self.status.set('CONTACTING UPDATE CHANNEL...')
        def worker():
            try:
                version=self.api.updater.update(self.root,lambda text,value=None:self.messages.put(('progress',text,value)),self_update=False,
                    select_collection=lambda:(self.messages.put(('collection','',None)),self.responses.get())[1])
                self.messages.put(('done','OPENHW '+version+' / All available updates installed.',1))
            except Exception as error:self.messages.put(('error',str(error),0))
        threading.Thread(target=worker,daemon=False).start()
    def cache(self):
        if self.dialog('REBUILD SHADER LIBRARY','Back up the current local cache? OpenHW will rebuild it at the next startup.',confirm=True):self.status.set(self.api.rebuild_shader_cache(self.root))
    def import_voice(self):
        self.api.updater.stop_if_game_running()
        folder=filedialog.askdirectory(parent=self.window,title='Select your localized Homeworld 1 Classic Data folder')
        if not folder:return
        language=self.dialog('VOICE ARCHIVE','Choose the actual language of this owned voice bank.',choices=self.api.LANGUAGES[1:])
        if not language:return
        selected=self.api.launcher_languages.import_classic(self.root,folder,language)
        control,display,options=self.choices['voiceLanguage'];options.clear();options.update(self.api.launcher_languages.available(self.root))
        control.configure(values=list(options.values()));self.variables['voiceLanguage'].set(str(selected));display.set(options[str(selected)])
        self.status.set('VOICE ARCHIVE IMPORTED / Save configuration to select it.')
    def poll(self):
        self.tick+=3
        while not self.messages.empty():
            kind,text,value=self.messages.get()
            if kind=='collection':self.responses.put(filedialog.askdirectory(parent=self.window,title='Select Homeworld Remastered Collection'));continue
            self.status.set(text);self.amount=value
            if kind!='progress':
                self.busy=False;self.config=self.api.Configuration(self.root);self.version=self.api.updater.detect_version(self.root);self.draw_rail()
                if kind=='error':self.dialog('UPDATE INTERRUPTED',text)
        self.draw_progress()
        self.window.after(100,self.poll)
    def popup(self,title,width=560,height=300):
        win=tk.Toplevel(self.window);win.withdraw();win.configure(bg=LINE);win.transient(self.window)
        result={'value':None}
        def finish(value=None):result['value']=value;win.destroy()
        win.chrome=Chrome(win,finish,title=title,height=52)
        content=tk.Frame(win,bg=PANEL,padx=24,pady=20);content.pack(fill='both',expand=True,padx=2,pady=(0,2))
        x=self.window.winfo_x()+(self.window.winfo_width()-width)//2;y=self.window.winfo_y()+(self.window.winfo_height()-height)//2
        win.geometry(f'{width}x{height}+{x}+{y}');self.modal=win
        return win,content,result,finish
    def show_popup(self,win):
        if self.preview:
            win.attributes('-alpha',0.0)
            user=ctypes.windll.user32;get=user.GetWindowLongPtrW;put=user.SetWindowLongPtrW
            get.argtypes=[wintypes.HWND,ctypes.c_int];get.restype=ctypes.c_ssize_t
            put.argtypes=[wintypes.HWND,ctypes.c_int,ctypes.c_ssize_t];put.restype=ctypes.c_ssize_t
            hwnd=win.chrome.hwnd();put(hwnd,-20,(get(hwnd,-20)|0x080000a0)&~0x40000)
        win.deiconify()
        if not self.preview:win.grab_set();win.focus_force()
        win.wait_window();self.modal=None
    def dialog(self,title,text,*,confirm=False,choices=None):
        win,content,result,finish=self.popup(title,height=320 if choices else 280)
        label(content,text,size=BODY,wraplength=500,justify='left').pack(fill='x',pady=(2,20))
        selected=None
        if choices:
            selected=tk.StringVar(value=choices[0]);ttk.Combobox(content,textvariable=selected,values=choices,state='readonly',font=BODY).pack(fill='x',pady=(0,20))
        actions=tk.Frame(content,bg=PANEL);actions.pack(side='bottom',fill='x')
        Button(actions,'CONFIRM' if confirm or choices else 'UNDERSTOOD',lambda:finish(selected.get() if selected else True),primary=True,width=220).pack(side='right')
        if confirm or choices:Button(actions,'CANCEL',lambda:finish(False),width=155).pack(side='left')
        win.bind('<Return>',lambda e:finish(selected.get() if selected else True));self.show_popup(win)
        return result['value']
    def pick_color(self,initial):
        win,content,result,finish=self.popup('PALETTE / CUSTOM COLOR',width=520,height=400)
        values=[tk.IntVar(value=int(initial[i:i+2],16)) for i in (1,3,5)];hexcode=tk.StringVar(value=initial.upper())
        swatch=tk.Canvas(content,height=68,bg=initial,highlightthickness=1,highlightbackground=LINE);swatch.pack(fill='x',pady=(0,18))
        sliders=[]
        def repaint(*args):
            color='#' + ''.join(f'{v.get():02x}' for v in values);hexcode.set(color.upper());swatch.configure(bg=color)
            for i,slider in enumerate(sliders):
                slider.delete('all');slider.create_text(0,18,text=('RED','GREEN','BLUE')[i],font=MONO,fill=MUTED,anchor='w')
                slider.create_rectangle(72,12,378,24,fill='#233f49',outline='');x=72+306*values[i].get()/255
                slider.create_rectangle(72,12,x,24,fill=(GOLD,CYAN,'#8fb2e0')[i],outline='');slider.create_line(x,6,x,30,fill=INK,width=2)
                slider.create_text(448,18,text=f'{values[i].get():03d}',font=MONO,fill=INK,anchor='e')
        for i,v in enumerate(values):
            slider=tk.Canvas(content,height=36,bg=PANEL,highlightthickness=0,cursor='hand2');slider.pack(fill='x');sliders.append(slider)
            def set_value(event,index=i):values[index].set(round(max(0,min(1,(event.x-72)/306))*255));repaint()
            slider.bind('<Button-1>',set_value);slider.bind('<B1-Motion>',set_value)
        actions=tk.Frame(content,bg=PANEL);actions.pack(fill='x',pady=(18,0))
        self.entry(actions,hexcode,width=10).pack(side='left')
        def apply():
            value=hexcode.get().strip().lstrip('#')
            if len(value)==6:
                try:int(value,16)
                except ValueError:return
                finish('#'+value)
        Button(actions,'APPLY COLOR',apply,width=210,primary=True).pack(side='right')
        repaint();win.bind('<Return>',lambda e:apply());self.show_popup(win)
        return result['value']
    def close(self):
        if self.busy:self.dialog('UPDATE IN PROGRESS','Let the update finish before closing the launcher.');return
        dirty=any(value.get()!=self.original[key] for key,value in self.variables.items()) or self.mods!=self.api.read_mods(self.root)
        if dirty and not self.dialog('UNSAVED CONFIGURATION','Close without saving your settings and mod loadout?',confirm=True):return
        self.window.destroy()

def run(root):
    import openhw_launcher as backend
    app=App(root,backend);app.window.mainloop()
