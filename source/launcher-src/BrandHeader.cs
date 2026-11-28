using System;using System.Drawing;using System.Drawing.Drawing2D;using System.IO;using System.Windows.Forms;
namespace FIFA08Launcher {
// Fit the viewport before WinForms computes scroll ranges. Always reserve the
// vertical gutter so showing it cannot push a child into horizontal overflow.
public class DarkFlowPanel:FlowLayoutPanel {
 public DarkFlowPanel(){DoubleBuffered=true;BackColor=Color.FromArgb(28,29,33);}
 protected override void OnLayout(LayoutEventArgs e){
  if(AutoScroll&&FlowDirection==FlowDirection.TopDown&&!WrapContents){
   int available=Math.Max(1,Math.Min(ClientSize.Width,Parent==null?ClientSize.Width:Parent.ClientSize.Width-14)-Padding.Horizontal-8);
   foreach(Control c in Controls)c.Width=Math.Max(1,available-c.Margin.Horizontal);
  }
  base.OnLayout(e);
 }
}
public class DarkScrollPanel:Panel {public DarkScrollPanel(){DoubleBuffered=true;BackColor=Color.FromArgb(28,29,33);}}
public class BrandHeader:Control {
 Image art;
 public BrandHeader(string path){
  SetStyle(ControlStyles.ResizeRedraw|ControlStyles.AllPaintingInWmPaint|ControlStyles.OptimizedDoubleBuffer,true);
  DoubleBuffered=true;
  try{if(File.Exists(path))art=Image.FromFile(path);}catch{}
  ForeColor=Color.White;
 }
 protected override void OnPaint(PaintEventArgs e){
  var g=e.Graphics;
  g.SmoothingMode=SmoothingMode.AntiAlias;
  g.InterpolationMode=InterpolationMode.HighQualityBicubic;
  g.TextRenderingHint=System.Drawing.Text.TextRenderingHint.ClearTypeGridFit;
  g.Clear(BackColor);
  int subH=22;
  int availLogoH=Math.Max(28,Height-subH-4);
  if(art!=null&&art.Width>0&&art.Height>0){
   float scale=Math.Min((float)(Width-24)/art.Width,(float)availLogoH/art.Height);
   int drawW=Math.Max(1,(int)(art.Width*scale));
   int drawH=Math.Max(1,(int)(art.Height*scale));
   int drawX=(Width-drawW)/2;
   int drawY=Math.Max(0,(availLogoH-drawH)/2);
   g.DrawImage(art,new Rectangle(drawX,drawY,drawW,drawH));
  }
  using(var authorFont=new Font("Segoe UI",9f,FontStyle.Regular))
  using(var yellowBrush=new SolidBrush(Color.FromArgb(185,187,195)))
  using(var whiteBrush=new SolidBrush(Color.FromArgb(225,225,230)))
  using(var sf=(StringFormat)StringFormat.GenericTypographic.Clone()){
   sf.FormatFlags|=StringFormatFlags.MeasureTrailingSpaces|StringFormatFlags.NoWrap;
   string p1="by ";
   string p2="Golazo Virtual";
   SizeF s1=g.MeasureString(p1,authorFont,PointF.Empty,sf);
   SizeF s2=g.MeasureString(p2,authorFont,PointF.Empty,sf);
   float totalW=s1.Width+s2.Width;
   float startX=(Width-totalW)/2f;
   float startY=Height-subH+2;
   g.DrawString(p1,authorFont,whiteBrush,startX,startY,sf);
   g.DrawString(p2,authorFont,yellowBrush,startX+s1.Width,startY,sf);
  }
 }
 protected override void Dispose(bool disposing){if(disposing&&art!=null)art.Dispose();base.Dispose(disposing);}
}
public class TileStrip:FlowLayoutPanel {public override Size GetPreferredSize(Size proposedSize){return new Size(Width,Height);}}
public class NavigationTab:Button {
 bool selected,hovered,attention;
 public bool Attention{get{return attention;}set{attention=value;Invalidate();}}
 public bool Selected{get{return selected;}set{selected=value;Invalidate();}}
 public NavigationTab(){SetStyle(ControlStyles.UserPaint|ControlStyles.AllPaintingInWmPaint|ControlStyles.OptimizedDoubleBuffer|ControlStyles.ResizeRedraw,true);FlatStyle=FlatStyle.Flat;FlatAppearance.BorderSize=0;UseVisualStyleBackColor=false;Cursor=Cursors.Hand;AccessibleRole=AccessibleRole.PageTab;}
 protected override void OnMouseEnter(EventArgs e){hovered=true;Invalidate();base.OnMouseEnter(e);}
 protected override void OnMouseLeave(EventArgs e){hovered=false;Invalidate();base.OnMouseLeave(e);}
 protected override void OnPaintBackground(PaintEventArgs e){e.Graphics.Clear(Parent==null?Color.FromArgb(36,37,41):Parent.BackColor);}
 protected override void OnPaint(PaintEventArgs e){
  OnPaintBackground(e);if(Width<12||Height<12)return;var g=e.Graphics;g.SmoothingMode=SmoothingMode.AntiAlias;
  if(selected||hovered||Attention)using(var path=UpdateNoticePill.Capsule(new Rectangle(3,3,Width-7,Height-7),16)){
   using(var fill=new SolidBrush(Attention?(hovered?Color.FromArgb(0,201,125):Color.FromArgb(0,201,125)):selected?Color.FromArgb(76,78,85):Color.FromArgb(48,50,56)))g.FillPath(fill,path);
   if(Attention)using(var edge=new Pen(Color.FromArgb(0,201,125)))g.DrawPath(edge,path);
  }
  TextRenderer.DrawText(g,Text,Font,new Rectangle(4,0,Width-8,Height),Attention?Color.FromArgb(25,62,44):selected?Color.White:Color.FromArgb(185,187,195),TextFormatFlags.HorizontalCenter|TextFormatFlags.VerticalCenter|TextFormatFlags.SingleLine|TextFormatFlags.EndEllipsis|TextFormatFlags.NoPrefix);
  if(Focused&&ShowFocusCues)ControlPaint.DrawFocusRectangle(g,new Rectangle(6,5,Width-12,Height-10));
 }
}
public class UpdateNoticePill:Button {
 bool hovered;
 public static GraphicsPath Capsule(Rectangle r,int diameter){
  int d=Math.Min(diameter,Math.Min(r.Width,r.Height));var p=new GraphicsPath();
  p.AddArc(r.Left,r.Top,d,d,180,90);p.AddArc(r.Right-d,r.Top,d,d,270,90);p.AddArc(r.Right-d,r.Bottom-d,d,d,0,90);p.AddArc(r.Left,r.Bottom-d,d,d,90,90);p.CloseFigure();return p;
 }
 protected override void OnSizeChanged(EventArgs e){base.OnSizeChanged(e);if(Width<8||Height<8)return;using(var path=Capsule(new Rectangle(0,0,Width,Height),Height)){var old=Region;Region=new Region(path);if(old!=null)old.Dispose();}}
 public UpdateNoticePill(){SetStyle(ControlStyles.UserPaint|ControlStyles.AllPaintingInWmPaint|ControlStyles.OptimizedDoubleBuffer|ControlStyles.ResizeRedraw,true);UseVisualStyleBackColor=false;FlatStyle=FlatStyle.Flat;FlatAppearance.BorderSize=0;Cursor=Cursors.Hand;BackColor=Color.FromArgb(0,201,125);}
 protected override void OnMouseEnter(EventArgs e){hovered=true;Invalidate();base.OnMouseEnter(e);}
 protected override void OnMouseLeave(EventArgs e){hovered=false;Invalidate();base.OnMouseLeave(e);}
 protected override void OnPaintBackground(PaintEventArgs e){}
 protected override void OnPaint(PaintEventArgs e){
  if(Width<8||Height<8)return;var g=e.Graphics;g.SmoothingMode=SmoothingMode.AntiAlias;g.Clear(Color.FromArgb(0,201,125));
  var r=new Rectangle(1,1,Width-3,Height-3);
  using(var shape=Capsule(r,r.Height)){
   using(var fill=new LinearGradientBrush(r,hovered?Color.FromArgb(0,201,125):Color.FromArgb(0,201,125),Color.FromArgb(0,201,125),90f))g.FillPath(fill,shape);
   using(var edge=new Pen(Focused||hovered?Color.FromArgb(42,102,70):Color.FromArgb(0,201,125)))g.DrawPath(edge,shape);
  }
  using(var glow=new SolidBrush(Color.FromArgb(28,34,91,61)))g.FillEllipse(glow,16,Height/2-9,18,18);
  using(var dot=new SolidBrush(Color.FromArgb(42,102,70)))g.FillEllipse(dot,21,Height/2-4,8,8);
  TextRenderer.DrawText(g,Text,Font,new Rectangle(42,0,Width-84,Height),Color.FromArgb(23,57,41),TextFormatFlags.HorizontalCenter|TextFormatFlags.VerticalCenter|TextFormatFlags.SingleLine|TextFormatFlags.EndEllipsis|TextFormatFlags.NoPrefix);
  using(var arrow=new Pen(Color.FromArgb(39,88,61),1.6f)){int x=Width-26,y=Height/2;g.DrawLine(arrow,x-3,y-4,x+1,y);g.DrawLine(arrow,x+1,y,x-3,y+4);}
 }
}
public class ChoiceTile:Button {
 bool hovered;
 public ChoiceTile(){SetStyle(ControlStyles.UserPaint|ControlStyles.AllPaintingInWmPaint|ControlStyles.OptimizedDoubleBuffer|ControlStyles.ResizeRedraw,true);UseVisualStyleBackColor=false;FlatStyle=FlatStyle.Flat;FlatAppearance.BorderSize=0;BackColor=Color.FromArgb(44,45,50);ForeColor=Color.FromArgb(210,212,219);Cursor=Cursors.Hand;}
 protected override void OnMouseEnter(EventArgs e){hovered=true;Invalidate();base.OnMouseEnter(e);}
 protected override void OnMouseLeave(EventArgs e){hovered=false;Invalidate();base.OnMouseLeave(e);}
 protected override void OnPaintBackground(PaintEventArgs e){}
 protected override void OnPaint(PaintEventArgs e){
  var g=e.Graphics;g.Clear(Parent==null?Color.FromArgb(28,29,33):Parent.BackColor);if(Width<8||Height<8)return;g.SmoothingMode=SmoothingMode.AntiAlias;
  bool active=BackColor.ToArgb()==PortUi.Accent.ToArgb();var r=new Rectangle(1,1,Width-3,Height-3);
  using(var path=UpdateNoticePill.Capsule(r,16)){
   using(var fill=new SolidBrush(!Enabled?Color.FromArgb(34,35,39):active?(hovered?Color.FromArgb(255,218,51):PortUi.Accent):hovered?Color.FromArgb(62,64,71):Color.FromArgb(44,45,50)))g.FillPath(fill,path);
   using(var edge=new Pen(active?Color.FromArgb(255,228,102):Color.FromArgb(62,64,71)))g.DrawPath(edge,path);
  }
  float size=Font.Size;while(size>7){using(var trial=new Font(Font.FontFamily,size,FontStyle.Regular)){if(TextRenderer.MeasureText(Text,trial).Width<=Width-16)break;}size-=.5f;}
  using(var font=new Font(Font.FontFamily,size,active?FontStyle.Bold:FontStyle.Regular))TextRenderer.DrawText(g,Text,font,r,!Enabled?Color.Gray:active?PortUi.AccentDark:ForeColor,TextFormatFlags.HorizontalCenter|TextFormatFlags.VerticalCenter|TextFormatFlags.SingleLine|TextFormatFlags.EndEllipsis|TextFormatFlags.NoPrefix);
  if(Focused&&ShowFocusCues)ControlPaint.DrawFocusRectangle(g,new Rectangle(6,5,Width-12,Height-10));
 }
}
public class PremiumPanel:Panel {
 public PremiumPanel(){SetStyle(ControlStyles.UserPaint|ControlStyles.AllPaintingInWmPaint|ControlStyles.OptimizedDoubleBuffer|ControlStyles.ResizeRedraw,true);BackColor=Color.FromArgb(28,29,33);Padding=new Padding(18);}
 protected override void OnPaintBackground(PaintEventArgs e){
  var g=e.Graphics;g.Clear(Parent==null?Color.FromArgb(20,21,25):Parent.BackColor);if(Width<20||Height<20)return;g.SmoothingMode=SmoothingMode.AntiAlias;
  using(var path=UpdateNoticePill.Capsule(new Rectangle(1,1,Width-3,Height-3),32)){
   using(var fill=new SolidBrush(BackColor))g.FillPath(fill,path);using(var pen=new Pen(Color.FromArgb(59,61,68)))g.DrawPath(pen,path);
  }
 }
}
}
