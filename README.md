# final-project-code-and-video-ryantoeung 
**Group Members**:  
* Cory Kim  
* Edward Hwang  
* Ryan Toeung  
  
## "Shower Time" an FPGA Game  
**50T vs 100T**  
Both versions of "Shower Time" work on the FPGA boards with minor differences  
 * 50T  
   * uses dummy core for the frame buffer so a background is not possible  
   * uses .txt for the bitmap files  
 * 100T  
   * implemented with the frame buffer so a background is possible  
   * uses .mem for the bitmap files  
  
**Video Demonstration**  
 * 50T: https://youtu.be/7wgvg_mUah4  
 * 100T: https://youtu.be/lUhOETvX21M  
  
**Description**  
The goal of this project is to create an FPGA game inspired by the mobile app "Temple Run". 
Our game "Shower Time" allows the player to control a cat whose goal is to avoid taking a shower. 
Once the cat is hit by a water droplet, the player loses and the number of drops dodged is scored. 
Our game has the following requirements: SPI, PS2, VGA, GPIO, SSEG, and timer. The SPI utilizes the 
onboard accelerometer to allow character movement. Moving the FPGA left will move the character to the left. 
Moving the FPGA right will move the character to the right. The PS2 uses a keyboard to enable user input for 
start, pause, play, and restart. The VGA displays the onscreen environment, obstacles, character, and the score. The 
GPO indicates to the user that the character has been hit with flashing LEDs. GPI allows the user to 
select the level difficulty with switch input in binary. The SSEG displays the current level difficulty. 
The timer keeps track of the time alotted for player movement before the obstacle moves down one row.  
  
**Features**  
* Inspired by the mobile app "Temple Run"  
* 1D character movement (cat)  
* Animated obstacle sprites (water)  
* Custom background (shower)  
* Different difficulties  
* Score & High Score  
  
2D Array: Stores character data, environment, and obstacles as int values  
0	0	1  
0	0	0  
0	1	0  
  
* 0 = Background  
* 1 = Character/Obstacle (Cat/Water)  
* int charPos = current character position  
  
Score: Keeps track total obstacles dodged  
Difficulty: Increases speed obstacles are added  
  
**Requirements:**  
-**SPI**: accelerometer controls character movement  
-**PS2 keyboard**: start, pause, play, and restart  
-**VGA**: moving sprite for our character, show environment, obstacles, and number of obstacles dodged  
-**GPO**: flash LEDs when character is hit  
-**GPI**: adjusts initial level speed  
-**SSEG**: display level difficulty  
-**Timer**: time alotted for player movement before the obstacle moves down  
  
**Modified HDL**
 * **video_sys_daisy.sv**: modified chu_vga_sprite_mouse_core  
ADDR_WIDTH(10)->ADDR_WIDTH(12)  
   * **chu_vga_sprite_ghost_core.sv**: repurposed ghost for water  
     * **ghost_src.sv**: repurposed ghost for water  
     * **ghost_ram.sv**: replaced "ghost_bitmap.txt" -> "water.txt"  
   * **chu_vga_sprite_mouse_core.sv**: repurposed 32x32 mouse for 64x64 cat  
     * **mouse_src.sv**: H_SIZE & V_SIZE = 64  
     Increased number of bits for addr_r = {yr[4:0], xr[4:0]} -> {yr[5:0], xr[5:0]}  
     * **moue_ram.sv**: replaced "mouse_pointer.txt" -> "cat.txt"  