using System;using System.Drawing;using System.Drawing.Drawing2D;using System.IO;using System.Windows.Forms;
namespace FIFA08Launcher {
// Fit the viewport before WinForms computes scroll ranges. Always reserve the
// vertical gutter so showing it cannot push a child into horizontal overflow.
public class DarkFlowPanel:FlowLayoutPanel {
 public DarkFlowPanel(){DoubleBuffered=true;BackColor=Color.FromArgb(17,17,20);}
 protected override void OnLayout(LayoutEventArgs e){
  if(AutoScroll&&FlowDirection==FlowDirection.TopDown&&!WrapContents){
   int available=Math.Max(1,Math.Min(ClientSize.Width,Parent==null?ClientSize.Width:Parent.ClientSize.Width-14)-Padding.Horizontal-8);
   foreach(Control c in Controls)c.Width=Math.Max(1,available-c.Margin.Horizontal);
  }
  base.OnLayout(e);
 }
}
public class DarkScrollPanel:Panel {public DarkScrollPanel(){DoubleBuffered=true;BackColor=Color.FromArgb(17,17,20);}}
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
  using(var authorFont=new Font("Segoe UI",10.5f,FontStyle.Bold))
  using(var yellowBrush=new SolidBrush(PortUi.Accent))
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
 bool selected,hovered;
 public bool Selected{get{return selected;}set{selected=value;Invalidate();}}
 public NavigationTab(){SetStyle(ControlStyles.UserPaint|ControlStyles.AllPaintingInWmPaint|ControlStyles.OptimizedDoubleBuffer|ControlStyles.ResizeRedraw,true);FlatStyle=FlatStyle.Flat;FlatAppearance.BorderSize=0;UseVisualStyleBackColor=false;Cursor=Cursors.Hand;AccessibleRole=AccessibleRole.PageTab;}
 protected override void OnMouseEnter(EventArgs e){hovered=true;Invalidate();base.OnMouseEnter(e);}
 protected override void OnMouseLeave(EventArgs e){hovered=false;Invalidate();base.OnMouseLeave(e);}
 protected override void OnGotFocus(EventArgs e){base.OnGotFocus(e);Invalidate();}
 protected override void OnLostFocus(EventArgs e){base.OnLostFocus(e);Invalidate();}
 protected override void OnPaintBackground(PaintEventArgs e){e.Graphics.Clear(Parent==null?Color.FromArgb(17,17,20):Parent.BackColor);}
 protected override void OnPaint(PaintEventArgs e){
  OnPaintBackground(e);if(Width<12||Height<12)return;e.Graphics.SmoothingMode=SmoothingMode.AntiAlias;
  if(selected||hovered){using(var path=new GraphicsPath()){const int d=10;path.AddArc(0,0,d,d,180,90);path.AddArc(Width-d-1,0,d,d,270,90);path.AddLine(Width-1,d,Width-1,Height);path.AddLine(Width-1,Height,0,Height);path.CloseFigure();using(var brush=new SolidBrush(selected?Color.FromArgb(17,17,20):Color.FromArgb(28,28,32)))e.Graphics.FillPath(brush,path);}}
  TextRenderer.DrawText(e.Graphics,Text,Font,new Rectangle(4,0,Width-8,Height),Enabled?(selected?PortUi.Accent:hovered?Color.White:Color.Silver):Color.DimGray,TextFormatFlags.HorizontalCenter|TextFormatFlags.VerticalCenter|TextFormatFlags.SingleLine|TextFormatFlags.EndEllipsis|TextFormatFlags.NoPrefix);
  if(Focused&&ShowFocusCues)ControlPaint.DrawFocusRectangle(e.Graphics,new Rectangle(5,4,Width-10,Height-8),Color.Silver,BackColor);
 }
}
public class ChoiceTile:Button {public ChoiceTile(){SetStyle(ControlStyles.UserPaint|ControlStyles.AllPaintingInWmPaint|ControlStyles.OptimizedDoubleBuffer|ControlStyles.ResizeRedraw,true);DoubleBuffered=true;UseVisualStyleBackColor=false;FlatStyle=FlatStyle.Flat;FlatAppearance.BorderSize=0;BackColor=Color.FromArgb(24,24,28);ForeColor=Color.Silver;Cursor=Cursors.Hand;}protected override void OnPaintBackground(PaintEventArgs e){e.Graphics.Clear(Parent==null?Color.FromArgb(16,16,19):Parent.BackColor);}protected override void OnPaint(PaintEventArgs e){e.Graphics.Clear(Parent==null?Color.FromArgb(16,16,19):Parent.BackColor);e.Graphics.SmoothingMode=SmoothingMode.AntiAlias;var r=new Rectangle(1,1,Width-3,Height-3);bool isSelected=BackColor.ToArgb()==PortUi.Accent.ToArgb();using(var p=new GraphicsPath()){int d=14;p.AddArc(r.Left,r.Top,d,d,180,90);p.AddArc(r.Right-d,r.Top,d,d,270,90);p.AddArc(r.Right-d,r.Bottom-d,d,d,0,90);p.AddArc(r.Left,r.Bottom-d,d,d,90,90);p.CloseFigure();using(var b=new SolidBrush(!Enabled?Color.FromArgb(20,20,22):isSelected?Color.FromArgb(62,PortUi.Accent):BackColor))e.Graphics.FillPath(b,p);using(var pen=new Pen(isSelected?PortUi.Accent:Color.FromArgb(48,48,53),isSelected?1.4f:1f))e.Graphics.DrawPath(pen,p);}float size=Font.Size;while(size>6.5f){using(var trial=new Font(Font.FontFamily,size,Font.Style)){if(TextRenderer.MeasureText(Text,trial,new Size(10000,10000),TextFormatFlags.NoPadding).Width<=r.Width-6&&TextRenderer.MeasureText(Text,trial,new Size(10000,10000),TextFormatFlags.NoPadding).Height<=r.Height-4)break;}size-=.5f;}using(var fitted=new Font(Font.FontFamily,size,Font.Style)){TextRenderer.DrawText(e.Graphics,Text,fitted,r,Enabled?(isSelected?PortUi.Accent:ForeColor):Color.DimGray,TextFormatFlags.HorizontalCenter|TextFormatFlags.VerticalCenter);}}}
public class PremiumPanel:Panel {
 public PremiumPanel(){SetStyle(ControlStyles.UserPaint|ControlStyles.AllPaintingInWmPaint|ControlStyles.OptimizedDoubleBuffer|ControlStyles.ResizeRedraw,true);BackColor=Color.FromArgb(17,17,20);Padding=new Padding(12);}
 protected override void OnPaintBackground(PaintEventArgs e){e.Graphics.Clear(Parent==null?Color.FromArgb(14,14,15):Parent.BackColor);var r=new Rectangle(1,1,Width-3,Height-3);if(r.Width<12||r.Height<12)return;using(var p=new GraphicsPath()){const int d=18;p.AddArc(r.Left,r.Top,d,d,180,90);p.AddArc(r.Right-d,r.Top,d,d,270,90);p.AddArc(r.Right-d,r.Bottom-d,d,d,0,90);p.AddArc(r.Left,r.Bottom-d,d,d,90,90);p.CloseFigure();e.Graphics.SmoothingMode=SmoothingMode.AntiAlias;using(var b=new LinearGradientBrush(r,Color.FromArgb(24,24,28),BackColor,90f))e.Graphics.FillPath(b,p);using(var pen=new Pen(Color.FromArgb(54,54,60)))e.Graphics.DrawPath(pen,p);}}
}}
