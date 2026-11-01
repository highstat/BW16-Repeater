/*
╔═╗   ╔═╗ ╔═╗           ╔═╗                   ╔═╗                   ╔═╗     
║ ║   ║ ║ ╚═╝ ╔═══════╗ ║ ╚═════╗   ╔═════╗ ╔═╝ ╚═╗   ╔═══════╗   ╔═╝ ╚═╗   
║ ╚═══╝ ║ ╔═╗ ║ ╔═══╗ ║ ║ ╔═══╗ ║ ╔═╝   ╔═╝ ╚═╗ ╔═╝   ║ ╔═══╗ ║   ╚═╗ ╔═╝   
║ ╔═══╗ ║ ║ ║ ║ ║   ║ ║ ║ ║   ║ ║ ╚═══╗ ╚═╗   ║ ║     ║ ║   ║ ║     ║ ║     
║ ║   ║ ║ ║ ║ ║ ╚═══╝ ║ ║ ║   ║ ║ ╔═══╝ ╔═╝   ║ ╚═══╗ ║ ╚═══╝ ╚═╗   ║ ╚═══╗ 
╚═╝   ╚═╝ ╚═╝ ╚═════╗ ║ ╚═╝   ╚═╝ ╚═════╝     ╚═════╝ ╚═════════╝   ╚═════╝ 
                ╔═══╝ ║                                                     
                ╚═════╝                                                     
                © Copyright 2026 
            `https://github.com/highstat/BW16-Repeater` 
    Name: Watchdog 
    Description: System watchdog interface 
    Author: @highstat 
    Platform: BW16 (RTL8720dn) - Ameba Arduino 
    License: MIT 
*/

#pragma once
class Watchdog {
 public:
  void begin();
  void feed();
};