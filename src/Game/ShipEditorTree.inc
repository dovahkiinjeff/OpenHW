/* Presentation folders never change the saved joint hierarchy. */
static struct {int count,node[SHIP_AUTHOR_POINTS+9],depth[SHIP_AUTHOR_POINTS+9];unsigned char folded[SHIP_AUTHOR_POINTS],folders[9];int drag,startX,startY;} speTree;
static void speTreeReset(void){memset(&speTree,0,sizeof(speTree));}
static const char*speFolders[]={"HULL / JOINTS","NAV LIGHTS","DOCKING / LAUNCH","SALVAGE","REPAIR","WEAPONS / TURRETS","TURRETS","ENGINES / TRAILS","SUBSYSTEMS"};
static int speTreeCategory(int i){int category=shipAuthorCategory(&spe.doc.points[i]),parent=shipAuthorParent(&spe.doc,i);unsigned depth=0;
 if(category==SAP_SUBSYSTEM)return SAP_SUBSYSTEM;
 {int ancestor=parent;unsigned n=0;while(ancestor>=0&&n++<spe.doc.pointCount){if(spe.doc.points[ancestor].subsystem[0])return SAP_SUBSYSTEM;ancestor=shipAuthorParent(&spe.doc,ancestor);}}
 if(category==SAP_TURRET)category=SAP_WEAPON;
 while(parent>=0&&depth++<spe.doc.pointCount){int c=shipAuthorCategory(&spe.doc.points[parent]);if(c==SAP_WEAPON||c==SAP_TURRET)return SAP_WEAPON;if(category==SAP_JOINT&&c!=SAP_JOINT)category=c;parent=shipAuthorParent(&spe.doc,parent);}
 return category;
}
static void speTreeBranch(int node,int depth,int category){unsigned j;if(speTree.count>=SHIP_AUTHOR_POINTS+9)return;
 speTree.node[speTree.count]=node;speTree.depth[speTree.count++]=depth;if(speTree.folded[node])return;
 for(j=0;j<spe.doc.pointCount;j++)if(shipAuthorParent(&spe.doc,j)==node&&speTreeCategory(j)==category)speTreeBranch(j,depth+1,category);
}
static void speTreeBuild(void){int category;unsigned j;speTree.count=0;
 for(category=0;category<9;category++){if(category==SAP_TURRET)continue;speTree.node[speTree.count]=-1-category;speTree.depth[speTree.count++]=0;
  if(speTree.folders[category])continue;
  for(j=0;j<spe.doc.pointCount;j++){int p=shipAuthorParent(&spe.doc,j);if(speTreeCategory(j)==category&&(p<0||speTreeCategory(p)!=category))speTreeBranch(j,1,category);}
 }
 spe.scroll=muiClamp(spe.scroll,0,speTree.count>0?speTree.count-1:0);
}
static int speTreeRow(int x,int y){int row;if(!muiPointIn(&mpeList,x,y)||y<speSource.y1+uiScaleSize(8))return -1;row=spe.scroll+(y-speSource.y1-uiScaleSize(8))/uiScaleSize(29);return row<speTree.count?row:-1;}
static void speTreeDraw(void){int row,visible=(mpeList.y1-speSource.y1-uiScaleSize(20))/uiScaleSize(29);speTreeBuild();
 for(row=spe.scroll;row<speTree.count&&row<spe.scroll+visible;row++){int n=speTree.node[row],indent=uiScaleSize(12*muiClamp(speTree.depth[row],0,6));char text[128];rectangle box=muiRect(mpeList.x0+3,speSource.y1+uiScaleSize(8+(row-spe.scroll)*29),mpeList.x1-3,speSource.y1+uiScaleSize(35+(row-spe.scroll)*29));
  if(n<0)snprintf(text,sizeof(text),"%s %s",speTree.folders[-1-n]?"[+]":"[-]",speFolders[-1-n]);
  else {unsigned j;int children=0;for(j=0;j<spe.doc.pointCount;j++)if(shipAuthorParent(&spe.doc,j)==n)children++;snprintf(text,sizeof(text),"%s %s%s",children?(speTree.folded[n]?"[+]":"[-]"):" . ",spe.doc.points[n].enabled?"":"(off) ",spe.doc.points[n].name);}
  muiDrawButtonRect(&box,"",n>=0&&n==spe.selected,muiPointIn(&box,mouseCursorX(),mouseCursorY()));
  /* Fit text to the panel rather than overflowing over the model. */
  {int chars=muiClamp((box.x1-box.x0-indent-uiScaleSize(12))/uiScaleSize(8),4,127);if((int)strlen(text)>chars){text[chars-1]='~';text[chars]=0;}}
  muiText(box.x0+indent+uiScaleSize(4),box.y0+uiScaleSize(7),n<0?muiSignal:muiPaper,text,muiBodyFont);
 }
}
static void speTreeDrop(int x,int y){int row=speTreeRow(x,y),source=speTree.drag-1;speTree.drag=0;
 if(source<0||row<0||abs(x-speTree.startX)+abs(y-speTree.startY)<uiScaleSize(8))return;
 if(shipAuthorReparent(&spe.doc,source,speTree.node[row]<0?-1:speTree.node[row])){speCheckpoint();speStatus("JOINT REPARENTED / WORLD POSITION PRESERVED");}else speStatus("INVALID PARENT / A JOINT CANNOT BE PARENTED TO ITS OWN DESCENDANT");
}
