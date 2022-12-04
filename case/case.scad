/*
 * ----------------------------------------------------------------------------
 * "THE BEER-WARE LICENSE" (Revision 42):
 * Jeroen Domburg <jeroen@spritesmods.com> wrote this file. As long as you retain 
 * this notice you can do whatever you want with this stuff. If we meet some day, 
 * and you think this stuff is worth it, you can buy me a beer in return. 
 * ----------------------------------------------------------------------------
 */

//Note that this is based on a design from RabbitEngineering: https://www.thingiverse.com/thing:3331061 which is licensed under CC-BY-NC.

disp_sz=[28,13.5, 1.5];

$fa=10;
$fs=0.1;

//Enable for the main PC model
pc_model();
//Enable for the bottom plate
//translate([0,0,-3]) bot_plate();

module pokey() {
    translate([0,0,.5+0.7]) sphere(d=1.8);
    cylinder(d=1.5, h=.5+.7);
}

module bot_plate() {
    hull() {
        translate([12,12.5,-.1]) cylinder(h=0.5, d=5);
        translate([-12,12.5,-.1]) cylinder(h=0.5, d=5);
        translate([12,-11,-.1]) cylinder(h=0.5, d=5);
        translate([-12,-11,-.1]) cylinder(h=0.5, d=5);
    }
    translate([12,10,-.1]) pokey();
    translate([-12,10,-.1]) pokey();
    translate([12,-10,-.1]) pokey();
    translate([-12,-10,-.1]) pokey();

}

module pc_model() {
    difference() {
        pc();
        //lcd
    translate([5.2,-11,1]) rotate([-8,0,0]) rotate([90,-90,0]) translate([-3,0,0])cube(disp_sz+[3.2, 0.2, 0.2]);    
        //hollow out
        scale(0.73) translate([0,-0,-0.1]) cube([26,20,70], center=true);
        //air/wire line
        translate([-0.5-5,-12,-0.1]) cube([1,100,1]);
        //switch
        translate([-4.1-5, 10+1.2, -10+2])cube([8.2, 3, 10]);
        translate([-3-5, 10, -10+2])cube([6, 4.2, 10]);
        //switch pokey bit
        translate([-1.7-5, 13, -10+2])cube([3.4, 4.2, 10]);
        //usb conn
        translate([-4.1+5, 9, -10+3])cube([8.2, 7, 10]);
        //usb line
        translate([-1+5, 7, -10+1])cube([2, 7, 10]);
        //usb conn holes
        translate([5,15-1.5, -10+3-2+5]) union() {
            translate([-2.2,0,5]) cube([0.5, 1.2, 10], center=true);
            translate([2.2,0,5]) cube([0.5, 1.2, 10], center=true);
        }
        //speaker grille
        translate([7,-4,4.1]) rotate([0,90,0]) union() {
            for(i=[0:45:360]) {
                rotate([0,0,i]) translate([0,-2.3,0]) cylinder(d=1.2, h=10);
            }
            translate([0,0,0]) cylinder(d=1.2, h=10);
        }
        //speaker
        translate([4.5,-4,4.1]) hull() {
            rotate([0,90,0]) cylinder(d=8.1, h=10);
            translate([0,0,-8]) rotate([0,90,0])  cylinder(d=8.1, h=10);
        }
        //holes for bottom plate
        translate([12,10,-.1]) cylinder(h=2, d1=1.8, d2=2.1);
        translate([-12,10,-.1]) cylinder(h=2, d1=1.8, d2=2.1);
        translate([12,-10,-.1]) cylinder(h=2, d1=1.8, d2=2.1);
        translate([-12,-10,-.1]) cylinder(h=2, d1=1.8, d2=2.1);
        //cut in two
        //translate([-1000,-500,-500]) cube(1000);
    }
}


module pc() {
    scale(0.73)translate([0,0,6.4]) union() {
        import("files/body_back.stl");
        translate([0,1,0]) import("files/body_face.stl");
        translate([0,1,0]) import("files/disks.stl");
        translate([0,0,22]) import("files/monitor.stl");
        translate([0,-14,22]) rotate([80,0,0])import("files/monitor_face.stl");
        //bit between monitor and px
        translate([0,-2,10]) cube([28,28.5,7], center=true);
        //shim between face and monitor?
        translate([-14,-12.9,29.5]) cube([28,2,3]);
    }
}

//translate([5.2,-11,1]) rotate([-8,0,0]) rotate([90,-90,0]) color("red",1.0) cube(disp_sz);