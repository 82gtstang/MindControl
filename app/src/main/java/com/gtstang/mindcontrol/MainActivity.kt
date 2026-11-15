package com.gtstang.mindcontrol
import android.app.Activity
import android.os.Bundle
import android.view.*
import android.graphics.*
import kotlin.random.Random
import kotlin.math.*
class MainActivity:Activity(){
 override fun onCreate(savedInstanceState:Bundle?){super.onCreate(savedInstanceState);setContentView(Board())}
 inner class Board:View(this){
  val r=Random(System.currentTimeMillis())
  val owner=IntArray(49);val troops=IntArray(49);val bonus=IntArray(49)
  val p=Paint(3);val colors=intArrayOf(0xff202b48.toInt(),0xff00d9ff.toInt(),0xffff477d.toInt(),0xffffc247.toInt())
  var selected=-1;var turn=1;var energy=5;var wins=getPreferences(0).getInt("wins",0);var msg="Select blue, then tap a neighbor";var over=false
  init{reset()}
  fun reset(){owner.fill(0);troops.fill(0);bonus.fill(0);val starts=mutableListOf(0,6,42,48);starts.shuffle(r);for(i in 0..2){owner[starts[i]]=i+1;troops[starts[i]]=8};repeat(12){bonus[r.nextInt(49)]=1+r.nextInt(3)};selected=-1;turn=1;energy=5;over=false;msg="Conquer the network!";invalidate()}
  fun label(c:Canvas,s:String,x:Float,y:Float,size:Float,color:Int=-1){p.color=color;p.style=Paint.Style.FILL;p.textSize=size;p.textAlign=Paint.Align.CENTER;p.typeface=Typeface.DEFAULT_BOLD;c.drawText(s,x,y,p)}
  fun button(c:Canvas,s:String,x:Float,y:Float,w:Float,color:Int){p.color=color;c.drawRoundRect(x,y,x+w,y+52,12f,12f,p);label(c,s,x+w/2,y+32,15f,Color.BLACK)}
  fun top()=max(160f,height*.22f)
  fun side()=(width-24f)/7f
  override fun onDraw(c:Canvas){c.drawColor(0xff090e22.toInt());val w=width.toFloat();val t=top();val z=side()
   label(c,"MIND CONTROL",w/2,65f,32f,colors[1]);label(c,"D O M I N A T I O N",w/2,91f,13f)
   label(c,"TURN "+turn,w*.17f,130f,16f);label(c,"ENERGY "+energy,w*.5f,130f,16f,colors[3]);label(c,"WINS "+wins,w*.84f,130f,16f)
   for(i in 0..48){val x=12+(i%7)*z;val y=t+(i/7)*z;p.color=colors[owner[i]];p.alpha=if(owner[i]==0)110 else 245;c.drawRoundRect(x+3,y+3,x+z-3,y+z-3,8f,8f,p);p.alpha=255
    if(selected==i){p.color=Color.WHITE;p.style=Paint.Style.STROKE;p.strokeWidth=4f;c.drawRoundRect(x+2,y+2,x+z-2,y+z-2,8f,8f,p);p.style=Paint.Style.FILL}
    if(owner[i]>0)label(c,troops[i].toString(),x+z/2,y+z*.64f,z*.38f,Color.BLACK) else if(bonus[i]>0)label(c,"✦",x+z/2,y+z*.64f,z*.38f,colors[3])
   }
   val b=t+7*z+24;label(c,msg,w/2,b,14f);button(c,"REINFORCE",12f,b+20,(w-36)/2,colors[1]);button(c,"END TURN",w/2+6,b+20,(w-36)/2,colors[2]);button(c,"NEW MAP",12f,b+82,(w-36)/2,colors[3]);button(c,"HOW TO PLAY",w/2+6,b+82,(w-36)/2,0xffa68aff.toInt())
  }
  fun adj(a:Int,b:Int)=a in 0..48&&b in 0..48&&abs(a/7-b/7)+abs(a%7-b%7)==1
  fun attack(a:Int,b:Int,faction:Int){val sent=troops[a]/2;if(sent<1)return;troops[a]-=sent;val strength=sent+r.nextInt(3);val defense=troops[b]+r.nextInt(2);if(strength>defense){owner[b]=faction;troops[b]=max(1,strength-defense);if(faction==1){energy+=bonus[b];msg="Captured node!"}}else{troops[b]=max(1,defense-strength);if(faction==1)msg="Attack repelled!"}}
  fun endTurn(){if(over)return;for(f in 2..3)repeat(2){val options=mutableListOf<Pair<Int,Int>>();for(a in 0..48)if(owner[a]==f&&troops[a]>=3)for(b in listOf(a-7,a+7,a-1,a+1))if(adj(a,b)&&owner[b]!=f)options.add(a to b);if(options.isNotEmpty()){val pair=options.random(r);attack(pair.first,pair.second,f)}}
   for(i in 0..48)if(owner[i]>0)troops[i]++;energy+=2+(0..48).count{owner[it]==1&&bonus[it]>0};turn++;selected=-1
   if((0..48).none{owner[it]==2||owner[it]==3}){over=true;wins++;getPreferences(0).edit().putInt("wins",wins).apply();msg="VICTORY! New map to replay"}else if(owner.none{it==1}){over=true;msg="DEFEAT! Try a new map"}else msg="Turn "+turn+" - capture the network"
  }
  override fun onTouchEvent(e:MotionEvent):Boolean{if(e.action!=MotionEvent.ACTION_UP)return true;val x=e.x;val y=e.y;val t=top();val z=side();val b=t+7*z+24
   if(y>=t&&y<t+7*z&&x>=12&&x<width-12&&!over){val i=((y-t)/z).toInt().coerceIn(0,6)*7+((x-12)/z).toInt().coerceIn(0,6);if(owner[i]==1){if(adj(selected,i)){val moved=troops[selected]/2;troops[selected]-=moved;troops[i]+=moved};selected=i}else if(adj(selected,i))attack(selected,i,1)}
   else if(y>=b+20&&y<=b+72){if(x<width/2){if(!over&&selected>=0&&owner[selected]==1&&energy>=3){energy-=3;troops[selected]+=5;msg="+5 troops deployed"}else msg="Select blue tile, need 3 energy"}else endTurn()}
   else if(y>=b+82&&y<=b+134){if(x<width/2)reset()else msg="Blue: select, attack neighbors. Reinforce costs 3."}
   invalidate();return true
  }
 }
}