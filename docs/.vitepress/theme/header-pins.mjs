// CAD pin centres, millimetres; front view with USB-C pointing down.
export function headerPinPosition(pin,z=8.5){return [13.847+2.54*pin.col,1.501-2.54*(pin.row-1),z]}
export function headerPinNumberAt(x,y,z){
 const col=Math.round((x-13.847)/2.54),row=Math.round((1.501-y)/2.54);
 if(col<0||col>2||row<0||row>7||z<2.45||z>8.6)return 0;
 if(Math.abs(x-(13.847+col*2.54))>.4||Math.abs(y-(1.501-row*2.54))>.4)return 0;
 return row*3+col+1;
}
