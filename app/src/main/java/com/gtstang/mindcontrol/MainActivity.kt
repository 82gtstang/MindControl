package com.gtstang.mindcontrol

import android.app.Activity
import android.os.Bundle
import android.graphics.*
import android.view.*
import kotlin.math.*
import kotlin.random.Random

class MainActivity : Activity() {
 override fun onCreate(savedInstanceState: Bundle?) {
  super.onCreate(savedInstanceState)
  window.decorView.systemUiVisibility = 4 or 1024 or 4096
  setContentView(Adventure())
 }
 inner class Adventure : View(this) {
  private val rng = Random(7291)
  private val world = Array(40) { IntArray(40) }
  private val paint = Paint(3)
  private val hero = Point(19, 20)
  private val enemies = mutableListOf<Point>()
  private val chests = mutableSetOf<Int>()
  private val opened = mutableSetOf<Int>()
  private val villagers = listOf(Point(20,19),Point(22,18),Point(18,22))
  private var hp=10
  private var gold=0
  private var potions=2
  private var keys=0
  private var kills=0
  private var quest=0
  private var facing=2
  private var dialog="Welcome to Ashenvale. Find the three ancient sigils."
  private var inventory=false
  private var gameOver=false
  private var victory=false
  private var tick=0
  private val colors=intArrayOf(0xff42864b.toInt(),0xff286b38.toInt(),0xff3674a6.toInt(),0xffa88c5c.toInt(),0xff4e4b57.toInt(),0xff3e8440.toInt(),0xffac9b72.toInt())
  private fun key(x:Int,y:Int)=y*40+x
  init { buildWorld() }
  private fun buildWorld(){
   for(y in 0..39)for(x in 0..39){
    val n=rng.nextInt(100)
    world[y][x]=when {
     x==0||y==0||x==39||y==39 -> 4
     n<8 -> 1
     n<13 -> 2
     n<19 -> 4
     n<27 -> 5
     else -> 0
    }
   }
   for(y in 16..24)for(x in 15..25)world[y][x]=if(x in 17..23&&y in 18..22)6 else 0
   for(x in 2..37){world[20][x]=3;world[21][x]=3}
   for(y in 2..37){world[y][20]=3;world[y][21]=3}
   for(y in 17..23)for(x in 17..23)world[y][x]=6
   for(npc in villagers)world[npc.y][npc.x]=6
   for(y in 18..22)for(x in 18..22)world[y][x]=6
   for(i in 0..2){val x=listOf(6,34,33)[i];val y=listOf(7,7,33)[i];world[y][x]=6;chests.add(key(x,y))}
   repeat(17){
    val x=rng.nextInt(3,37);val y=rng.nextInt(3,37)
    if(world[y][x] in listOf(0,3,5)&&abs(x-19)+abs(y-20)>8)enemies.add(Point(x,y))
   }
  }
  private fun text(c:Canvas,s:String,x:Float,y:Float,size:Float,color:Int=Color.WHITE,align:Paint.Align=Paint.Align.CENTER){
   paint.style=Paint.Style.FILL;paint.color=color;paint.textSize=size;paint.typeface=Typeface.create("sans-serif",Typeface.BOLD);paint.textAlign=align
   c.drawText(s,x,y,paint)
  }
  private fun rect(c:Canvas,x:Float,y:Float,w:Float,h:Float,color:Int,r:Float=0f){
   paint.color=color;paint.style=Paint.Style.FILL;c.drawRoundRect(x,y,x+w,y+h,r,r,paint)
  }
  private fun drawTile(c:Canvas,x:Float,y:Float,z:Float,t:Int,wx:Int,wy:Int){
   rect(c,x,y,z+1,z+1,colors[t])
   when(t){
    1,5 -> {
     rect(c,x+z*.42f,y+z*.46f,z*.16f,z*.44f,0xff674529.toInt(),3f)
     paint.color=if(t==1)0xff164c2c.toInt() else 0xff235c36.toInt()
     c.drawCircle(x+z*.5f,y+z*.38f,z*.32f,paint)
     c.drawCircle(x+z*.32f,y+z*.5f,z*.23f,paint)
     c.drawCircle(x+z*.67f,y+z*.5f,z*.23f,paint)
    }
    2 -> {paint.color=0xff8fd1ef.toInt();c.drawCircle(x+z*.4f,y+z*.55f,z*.12f,paint)}
    4 -> {paint.color=0xff77717b.toInt();c.drawCircle(x+z*.48f,y+z*.46f,z*.34f,paint)}
    6 -> {rect(c,x+z*.15f,y+z*.12f,z*.7f,z*.75f,0xffc3aa78.toInt(),4f)}
   }
   if(chests.contains(key(wx,wy))&&!opened.contains(key(wx,wy))){
    rect(c,x+z*.18f,y+z*.37f,z*.64f,z*.4f,0xff6a321b.toInt(),3f)
    rect(c,x+z*.2f,y+z*.34f,z*.6f,z*.13f,0xffe9b84e.toInt(),3f)
   }
  }
  private fun drawActor(c:Canvas,x:Float,y:Float,z:Float,color:Int,player:Boolean){
   paint.color=0x77000000;c.drawOval(x+z*.2f,y+z*.77f,x+z*.8f,y+z*.94f,paint)
   rect(c,x+z*.3f,y+z*.42f,z*.4f,z*.4f,color,5f)
   paint.color=0xffffd7a4.toInt();c.drawCircle(x+z*.5f,y+z*.33f,z*.19f,paint)
   if(player){rect(c,x+z*.25f,y+z*.2f,z*.5f,z*.1f,0xffbcd6df.toInt(),3f)}
   else {rect(c,x+z*.39f,y+z*.28f,z*.07f,z*.05f,0xff171717.toInt());rect(c,x+z*.56f,y+z*.28f,z*.07f,z*.05f,0xff171717.toInt())}
  }
  override fun onDraw(c:Canvas){
   c.drawColor(0xff101823.toInt())
   val w=width.toFloat();val h=height.toFloat()
   val z=min(w/10f,h*.068f)
   val top=h*.12f
   val offsetX=w/2-z*5f
   val offsetY=top
   text(c,"ASHENVALE: THE LOST SIGILS",w/2,34f,21f,0xffffd177.toInt())
   text(c,"HP $hp/10    GOLD $gold    SIGILS $quest/3",w/2,65f,16f)
   c.save();c.clipRect(0f,top,w,top+z*10)
   for(dy in 0..9)for(dx in 0..9){
    val wx=hero.x+dx-5;val wy=hero.y+dy-5
    if(wx in 0..39&&wy in 0..39)drawTile(c,offsetX+dx*z,offsetY+dy*z,z,world[wy][wx],wx,wy)
   }
   for(npc in villagers){val dx=npc.x-hero.x+5;val dy=npc.y-hero.y+5;if(dx in 0..9&&dy in 0..9)drawActor(c,offsetX+dx*z,offsetY+dy*z,z,0xffc783e9.toInt(),false)}
   for(e in enemies){val dx=e.x-hero.x+5;val dy=e.y-hero.y+5;if(dx in 0..9&&dy in 0..9)drawActor(c,offsetX+dx*z,offsetY+dy*z,z,0xffc43c47.toInt(),false)}
   drawActor(c,offsetX+5*z,offsetY+5*z,z,0xff347de3.toInt(),true)
   c.restore()
   val bottom=top+z*10
   rect(c,10f,bottom+8,w-20,57f,0xff27344b.toInt(),10f)
   text(c,dialog.take(51),w/2,bottom+32,13f)
   if(dialog.length>51)text(c,dialog.drop(51).take(51),w/2,bottom+49,13f)
   val ctrl=bottom+85
   fun button(s:String,x:Float,y:Float,ww:Float,hh:Float,col:Int){rect(c,x,y,ww,hh,col,13f);text(c,s,x+ww/2,y+hh*.65f,19f)}
   val b=min(w*.145f,64f)
   button("▲",w*.22f,ctrl,b,b,0xff405879.toInt())
   button("◀",w*.22f-b-5,ctrl+b+5,b,b,0xff405879.toInt())
   button("▼",w*.22f,ctrl+b+5,b,b,0xff405879.toInt())
   button("▶",w*.22f+b+5,ctrl+b+5,b,b,0xff405879.toInt())
   button("⚔",w*.68f,ctrl,b,b,0xffa33c4e.toInt())
   button("USE",w*.68f,ctrl+b+5,b,b,0xff54794c.toInt())
   button("BAG",w*.68f-b-5,ctrl+b+5,b,b,0xff8b6e3e.toInt())
   if(inventory||gameOver||victory){
    rect(c,18f,top+z,w-36,z*7,0xee111d30.toInt(),18f)
    if(inventory){
     text(c,"INVENTORY",w/2,top+z*2,28f,0xffffd177.toInt())
     text(c,"Potions: $potions",w/2,top+z*3,20f)
     text(c,"Gold: $gold",w/2,top+z*4,20f)
     text(c,"Ancient sigils: $quest/3",w/2,top+z*5,20f)
     text(c,"Tap BAG to close",w/2,top+z*7,16f)
    }else{
     text(c,if(victory)"QUEST COMPLETE!" else "YOU HAVE FALLEN",w/2,top+z*3,25f,0xffffd177.toInt())
     text(c,"Tap anywhere to start again",w/2,top+z*5,16f)
    }
   }
  }
  private fun passable(x:Int,y:Int)=x in 1..38&&y in 1..38&&world[y][x] !in listOf(1,2,4,5)
  private fun move(dx:Int,dy:Int){
   if(inventory||gameOver||victory)return
   facing=when{dy<0->0;dx>0->1;dy>0->2;else->3}
   val x=hero.x+dx;val y=hero.y+dy
   if(enemies.any{it.x==x&&it.y==y}){dialog="A shadow blocks your way. Use your sword.";return}
   if(passable(x,y)&&villagers.none{it.x==x&&it.y==y}){hero.x=x;hero.y=y;dialog="Explore the woods, ruins and village."}
   else if(villagers.any{it.x==x&&it.y==y})dialog=when{
    quest==0->"Elder: Three sigils are hidden in ancient chests."
    quest<3->"Villager: Search the northwest, northeast and southeast."
    else->"Elder: You have restored the light to Ashenvale!"
   }
   else dialog="The path is blocked."
   if(chests.contains(key(x,y))&&!opened.contains(key(x,y))){opened.add(key(x,y));quest++;gold+=25;dialog="You found an ancient sigil! ($quest/3)";if(quest==3){victory=true;dialog="All three sigils found!"}}
   if(rng.nextInt(6)==0)enemyTurn()
  }
  private fun enemyTurn(){
   if(enemies.any{abs(it.x-hero.x)+abs(it.y-hero.y)==1}){
    hp--;dialog="A shadow strikes! HP $hp/10"
    if(hp<=0){gameOver=true;dialog="The shadows have won."}
   }
  }
  private fun action(){
   if(inventory||gameOver||victory)return
   val dx=intArrayOf(0,1,0,-1)[facing];val dy=intArrayOf(-1,0,1,0)[facing]
   val target=enemies.indexOfFirst{abs(it.x-hero.x)+abs(it.y-hero.y)==1&&(it.x==hero.x+dx&&it.y==hero.y+dy)}
   if(target>=0){enemies.removeAt(target);kills++;gold+=5;dialog="Shadow defeated! +5 gold";if(kills%4==0){potions++;dialog="Shadow defeated! Found a potion."}}
   else if(villagers.any{abs(it.x-hero.x)+abs(it.y-hero.y)==1})dialog="Elder: Recover the three ancient sigils."
   else dialog="Your sword cuts through the air."
   enemyTurn()
  }
  private fun restart(){hero.x=19;hero.y=20;hp=10;gold=0;potions=2;keys=0;kills=0;quest=0;inventory=false;gameOver=false;victory=false;opened.clear();enemies.clear();repeat(15){val x=rng.nextInt(3,37);val y=rng.nextInt(3,37);if(passable(x,y)&&abs(x-19)+abs(y-20)>8)enemies.add(Point(x,y))};dialog="A new adventure begins!"}
  override fun onTouchEvent(e:MotionEvent):Boolean{
   if(e.action!=MotionEvent.ACTION_UP)return true
   if(gameOver||victory){restart();invalidate();return true}
   val w=width.toFloat();val h=height.toFloat();val z=min(w/10f,h*.068f)
   val ctrl=h*.12f+z*10+85
   val b=min(w*.145f,64f);val x=e.x;val y=e.y
   fun hit(bx:Float,by:Float)=x>=bx&&x<=bx+b&&y>=by&&y<=by+b
   when{
    hit(w*.22f,ctrl)->move(0,-1)
    hit(w*.22f-b-5,ctrl+b+5)->move(-1,0)
    hit(w*.22f,ctrl+b+5)->move(0,1)
    hit(w*.22f+b+5,ctrl+b+5)->move(1,0)
    hit(w*.68f,ctrl)->action()
    hit(w*.68f,ctrl+b+5)->{if(potions>0&&hp<10){potions--;hp=min(10,hp+5);dialog="Potion restores your health!"}else dialog="No potion needed or available."}
    hit(w*.68f-b-5,ctrl+b+5)->inventory=!inventory
   }
   invalidate();return true
  }
 }
}
