function Vectorizer() {
	var that = this;
	
	function sameColor(px1, px2) {
		return px1 && px2 && px1[0] == px2[0] && px1[1] == px2[1] && px1[2] == px2[2] && px1[3] == px2[3];
	}
	function getPixel(imd, x, y) {
		if (x < 0 || y < 0 || x > that.cnv.width || y > that.cnv.height) {
			return;
		}
		var px = that.ctx.getImageData(x, y, 1, 1);
		return px;
	}
	function getPixelColor(imd, x, y) {
		var i = y * imd.width * 4 + x * 4, px = imd.data[i];
		if (px == undefined) {
			return false;
		}
		px = [px, imd.data[i+1], imd.data[i+2], imd.data[i+3]];
		return px;
	}
	function putPixel(x, y, fillColor) {
		var d = getPixel(x, y);
		d.data[0] = fillColor[0];
		d.data[1] = fillColor[1];
		d.data[2] = fillColor[2];
		d.data[3] = fillColor[3];
		that.ctx.putImageData(d, x, y);
	}
	function m(p1,p2){return (p2.y-p1.y)/(p2.x-p1.x);}
	function renderHorizontalLine(minX, maxX, y, fillColor) {
		var c = that.ctx;
		c.fillStyle='rgba('+fillColor.join(',')+')';
		c.fillRect(minX, y, maxX - minX + 1, 1);
		var p1={x:minX,y:y},p2={x:maxX,y:y},
			_m=m(p1,p2),
			s = 15,
			_x = s * Math.cos(Math.PI / 2 - Math.atan(_m)),
			_y = s * Math.sin(Math.PI / 2 - Math.atan(_m));
		c.fillRect(minX-_x,y-_y,maxX-_x-(minX-_x,y-_y)+1,1)
	}

	function checkAbove(imd,x,y,targetColor,ydiff) {
		var abv = [];
		if(!sameColor(getPixelColor(imd,x-1,y),targetColor)){
			abv.push({x:x-1,y:y});
		}
		if(!sameColor(getPixelColor(imd,x,y),targetColor)){
			abv.push({x:x,y:y});
		}
		if(!sameColor(getPixelColor(imd,x+1,y),targetColor)){
			abv.push({x:x+1,y:y});
		}
		return abv.length ? abv : false;
	}

	function scanLine(imd, px, py, targetColor, not) {
		var x = px, y = py, minX, maxX, eq, _abv = [], _blw = [];

		eq = sameColor(getPixelColor(imd, x, y), targetColor);
		while (x > 0 && (not ? !eq : eq)) {
			if (not) {
				var abv=checkAbove(imd,x,y-1,targetColor);
				if(abv) {
					_abv.push.apply(_abv, abv);
				}
				var blw=checkAbove(imd,x,y+1,targetColor);
				if(blw) {
					_blw.push.apply(_blw, blw);
				}
			}
			x--;
			eq = sameColor(getPixelColor(imd, x, y), targetColor);
		}
		minX = x+1;
		
		x = px + 1;
		
		eq = sameColor(getPixelColor(imd, x, y), targetColor);
		while (x < that.cnv.width && (not ? !eq : eq)) {
			if (not) {
				var abv=checkAbove(imd,x,y-1,targetColor);
				if(abv) {
					_abv.push.apply(_abv, abv);
				}
				var blw=checkAbove(imd,x,y+1,targetColor);
				if(blw) {
					_blw.push.apply(_blw, blw);
				}
			}
			x++;
			eq = sameColor(getPixelColor(imd, x, y), targetColor);
		}
		maxX = x-1;

		return {minX: minX, maxX: maxX, _abv: _abv, _blw: _blw};
	}
	
	function floodFill(imd, px, py, targetColor, fillColor, dir, second) {
		if(!dir){
			floodFill.mat={};
			floodFill.knots=[];
		}
		if(floodFill.mat[py]&&floodFill.mat[py][px]){
			return;
		}
		if (px <= 0 || py <= 0 || px >= that.cnv.width || py >= that.cnv.height || sameColor(getPixelColor(imd, px, py), fillColor)) {
			return;
		}

		var y = py;
		
		that.ctx.strokeStyle = 'rgba(' + fillColor.join(',') + ')';
		that.ctx.fillStyle = 'rgba(' + fillColor.join(',') + ')';
		
		var r = scanLine(imd,px,py,targetColor,true),
			minX = r.minX, maxX = r.maxX;
		
		//renderHorizontalLine(r.minX,r.maxX,y,fillColor);
		var newKnots = [];
		for(var i=r.minX;i<=r.maxX;i++){
			if(!floodFill.mat[y]){
				floodFill.mat[y]={};
			}
			floodFill.mat[y][i]=true;
			newKnots.push({x:i,y:y});
			//nc.getContext('2d').fillRect(i,y,1,1);
		}
		var rv=false;
		if(Math.abs(px-minX)>Math.abs(px-maxX)) {
			newKnots.reverse();
			rv=true;
		}
		newKnots.forEach(function (n) {
			floodFill.knots.push(n);
		});
		var l=floodFill.knots[floodFill.knots.length-1];
		if((r._abv.length+r._blw.length)>2) {
			fillColor = [0,255,0,255];
		}
		if(r._abv.length){
			r._abv.sort(function(a,b){return a.x-b.x;});
			//if(Math.abs(px-r._abv[0].x)<Math.abs(px-r._abv[r._abv.length-1].x))r._abv.reverse();
			//var f=floodFill.knots[floodFill.knots.length-1];
			//var _abv = r._abv[0];
			r._abv.forEach(function (_abv,i) {
				//if(i>0)floodFill.knots.push(f);
				floodFill(imd, _abv.x, y-1, targetColor, fillColor, 'UP',!dir);
			});
		}
		if(r._blw.length){
			r._blw.sort(function(a,b){return b.x-a.x;});
			//if(Math.abs(px-r._blw[0].x)<Math.abs(px-r._blw[r._blw.length-1].x))r._blw.reverse();
			//var _blw = r._blw[0];
			//var f=floodFill.knots[floodFill.knots.length-1];
			r._blw.forEach(function (_blw) {
				//if(i>0)floodFill.knots.push(f);
				floodFill(imd, _blw.x, y+1, targetColor, fillColor, 'DOWN',!dir);
			});
		}
	}
	floodFill.q=[];
	
	return {
		init: function (cnv) {
			that.cnv = cnv;
			that.ctx = that.cnv.getContext('2d');
			
			that.ctx.lineWidth = 3;
			
			var prevPoint, hdlrMouseMove = function (e) {
				plotLine(prevPoint.x,prevPoint.y,e.pageX,e.pageY,that);
				
				prevPoint = {x: e.pageX, y: e.pageY};
			}, hdlrMouseUp = function (e) {
				removeEventListener('mousemove', hdlrMouseMove);
				removeEventListener('mouseup', hdlrMouseUp);
				
				window.mup=true;
			};
			that.cnv.addEventListener('mousedown', function (e) {
				var bbox = this.getBoundingClientRect();
				prevPoint = {x: Math.round(e.pageX - bbox.left), y: Math.round(e.pageY - bbox.top)};
				
				//if(window.mup){
					var imd=that.ctx.getImageData(0, 0, that.cnv.width, that.cnv.height),
					r=scanLine(imd, prevPoint.x, prevPoint.y, [255,255,255,255]),maxX=r.maxX;
					
					floodFill(imd, r.minX-1, prevPoint.y, [255,255,255,255], [255,0,0,255]);
					//floodFill(imd, r.maxX+1, prevPoint.y, [255,255,255,255], [255,0,0,255]);
					function sqDist(point1, point2) {
						return (point1.x - point2.x) * (point1.x - point2.x) + (point1.y - point2.y) * (point1.y - point2.y)
					}
					let knots = floodFill.knots
					let knotCount = knots.length
					let sets = []
					let curSet = []
					for (let idx = 0, len = knotCount; idx < len; idx++) {
						let knot = knots[idx]
						if (idx > 0 && idx < knotCount - 1) {
							if (sqDist(knots[idx-1], knot) > 8) {
								if (curSet.length) {
									sets.push(curSet)
								}
								curSet = []
							} else {
								curSet.push(knot)
							}
						}
						else if (idx > 0) {
							if (!(sqDist(knots[idx-1], knot) <= 8)) {
								if (curSet.length) {
									sets.push(curSet)
								}
								curSet = []
							} else {
								curSet.push(knot)
							}
						}
						else if (idx < knotCount - 1) {
							if (!(sqDist(knots[idx+1], knot) <= 8)) {
								if (curSet.length) {
									sets.push(curSet)
								}
								curSet = []
							} else {
								curSet.push(knot)
							}
						}
					}
					if (curSet.length) {
						sets.push(curSet)
					}
					sets.forEach((set) => {
						//let newSet = new sniktaPresentation.classes.Scribble(Array.from(set))
					})
					console.log(Array.from(sets.map((set) => Array.from(set))))

					while (sets.length > 1) {
						let i = 0
						let closestToStart = Array.from(sets.filter((set, idx) => idx != i)).sort((a, b) => {
							return Math.min(sqDist(a[0], sets[i][0]), sqDist(a[a.length-1], sets[i][0]))
									< Math.min(sqDist(b[0], sets[i][0]), sqDist(b[b.length-1], sets[i][0])) ? -1 : 1;
						})[0]
						let closestToEnd = Array.from(sets.filter((set, idx) => idx != i)).sort((a, b) => {
							return Math.min(sqDist(a[0], sets[i][sets[i].length-1]), sqDist(a[a.length-1], sets[i][sets[i].length-1]))
									< Math.min(sqDist(b[0], sets[i][sets[i].length-1]), sqDist(b[b.length-1], sets[i][sets[i].length-1])) ? -1 : 1;
						})[0]
						let newSet
						let startDist = Math.min(sqDist(closestToStart[0], sets[i][0]), sqDist(closestToStart[closestToStart.length-1], sets[i][0]))
						let endDist = Math.min(sqDist(closestToEnd[0], sets[i][sets[i].length-1]), sqDist(closestToEnd[closestToEnd.length-1], sets[i][sets[i].length-1]))
						console.log(Array.from(sets.map((set) => Array.from(set))), startDist, endDist)
						if (startDist < endDist) { // closer to start
							if (sqDist(closestToStart[0], sets[i][0]) < sqDist(closestToStart[closestToStart.length-1], sets[i][0])) {
								closestToStart.reverse()
							}
							newSet = closestToStart.concat(sets[i])
							sets.splice(i, 1)
							sets.splice(sets.indexOf(closestToStart), 1)
						} else { // closer to end
							if (sqDist(closestToEnd[closestToEnd.length-1], sets[i][sets[i].length-1]) < sqDist(closestToEnd[0], sets[i][sets[i].length-1])) {
								closestToEnd.reverse()
							}
							newSet = sets[i].concat(closestToEnd)
							sets.splice(i, 1)
							sets.splice(sets.indexOf(closestToEnd), 1)
						}
						sets.push(newSet)
					}
					
					//let newSet = new sniktaPresentation.classes.Scribble(Array.from(sets[0]))
					
					knots = sets[0]
					knotCount = knots.length
					sets = []
					curSet = []
					for (let idx = 0, len = knotCount; idx < len; idx++) {
						let knot = knots[idx]
						if (idx > 0 && idx < knotCount - 1) {
							if (sqDist(knots[idx-1], knot) > 8) {
								if (curSet.length) {
									sets.push(curSet)
								}
								curSet = []
							} else {
								curSet.push(knot)
							}
						}
						else if (idx > 0) {
							if (!(sqDist(knots[idx-1], knot) <= 8)) {
								if (curSet.length) {
									sets.push(curSet)
								}
								curSet = []
							} else {
								curSet.push(knot)
							}
						}
						else if (idx < knotCount - 1) {
							if (!(sqDist(knots[idx+1], knot) <= 8)) {
								if (curSet.length) {
									sets.push(curSet)
								}
								curSet = []
							} else {
								curSet.push(knot)
							}
						}
					}
					if (curSet.length) {
						sets.push(curSet)
					}
					
					while (sets.length > 1) {
						let i = 0
						let closestToStart = Array.from(sets.filter((set, idx) => idx != i)).sort((a, b) => {
							return Math.min(sqDist(a[0], sets[i][0]), sqDist(a[a.length-1], sets[i][0]))
									< Math.min(sqDist(b[0], sets[i][0]), sqDist(b[b.length-1], sets[i][0])) ? -1 : 1;
						})[0]
						let closestToEnd = Array.from(sets.filter((set, idx) => idx != i)).sort((a, b) => {
							return Math.min(sqDist(a[0], sets[i][sets[i].length-1]), sqDist(a[a.length-1], sets[i][sets[i].length-1]))
									< Math.min(sqDist(b[0], sets[i][sets[i].length-1]), sqDist(b[b.length-1], sets[i][sets[i].length-1])) ? -1 : 1;
						})[0]
						let newSet
						let startDist = Math.min(sqDist(closestToStart[0], sets[i][0]), sqDist(closestToStart[closestToStart.length-1], sets[i][0]))
						let endDist = Math.min(sqDist(closestToEnd[0], sets[i][sets[i].length-1]), sqDist(closestToEnd[closestToEnd.length-1], sets[i][sets[i].length-1]))
						console.log(Array.from(sets.map((set) => Array.from(set))), startDist, endDist)
						if (startDist < endDist) { // closer to start
							if (sqDist(closestToStart[0], sets[i][0]) < sqDist(closestToStart[closestToStart.length-1], sets[i][0])) {
								closestToStart.reverse()
							}
							newSet = closestToStart.concat(sets[i])
							sets.splice(i, 1)
							sets.splice(sets.indexOf(closestToStart), 1)
						} else { // closer to end
							if (sqDist(closestToEnd[closestToEnd.length-1], sets[i][sets[i].length-1]) < sqDist(closestToEnd[0], sets[i][sets[i].length-1])) {
								closestToEnd.reverse()
							}
							newSet = sets[i].concat(closestToEnd)
							sets.splice(i, 1)
							sets.splice(sets.indexOf(closestToEnd), 1)
						}
						sets.push(newSet)
					}

					sets.forEach((set) => {
						let newSet = new sniktaPresentation.classes.Scribble(Array.from(set))
					})
					
					sniktaPresentation.importantVariables.currentPresentation.currentSlide.redraw();
					//sniktaPresentation.dialogs.dlgVectorizer.content.innerHTML='';
					//sniktaPresentation.dialogs.dlgVectorizer.content.appendChild(nc);
					/*floodFill.knots.forEach(function (k,i) {
						setTimeout(function(){nc.getContext('2d').fillRect(k.x, k.y, 1, 1);},i*50);
					});*/
					
					//sniktaPresentation.dialogs.dlgVectorizer.hide();
					//console.log(JSON.stringify(floodFill.knots));
					
					//window.mup=false;
				//}
				
				//addEventListener('mousemove', hdlrMouseMove);
				//addEventListener('mouseup', hdlrMouseUp);
			});
		}
	};
}

function generateVectorizerDialog() {
	var myVectorizer = new Vectorizer,
		dlgVectorizer = sniktaPresentation.dialogs.dlgVectorizer = new sniktaPresentation.classes.Dialog({
			title: 'Vectorizer',
			content: addEl('div', {style:'padding-top:1em;'},addEl('input', {type: 'file', __onchange: function (e) {
				var fReader = new FileReader();
				fReader.addEventListener('loadend', function () {
					img = addEl('img', {src: this.result});
					img.onload = function () {
						var cnv = document.createElement('canvas');
						cnv.width = img.width;
						cnv.height = img.height;
						dlgVectorizer.content.appendChild(cnv);
						cnv.getContext('2d').drawImage(this,0,0);
						myVectorizer.init(cnv);
					};
				});
				fReader.readAsDataURL(this.files[0]);
			}})),
			size: {width: 50, height: 70},
		});
	sniktaPresentation.chrome.dialogsContainer.appendChild(dlgVectorizer.element);
	
	dlgVectorizer.show();
}