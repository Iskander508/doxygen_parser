var Graph = Graph || (function(){
	return {
        draw : function(data, baseDir) {
			var elems = {
				nodes: [],
				edges: []
			};

			var nodes = data.nodes;
			for(var i in nodes) {
				var newNode = {
					id: nodes[i].id,
					shortName: nodes[i].shortName,
					longName: nodes[i].longName,
					type: nodes[i].type
				};
				if (nodes[i].hasOwnProperty('parent')){
					newNode.parent = nodes[i].parent;
				}
				if (nodes[i].hasOwnProperty('reference')){
					newNode.reference = nodes[i].reference;
				}
				var nodeClasses = nodes[i].type;
				if (nodes[i].hasOwnProperty('classes')) {
					for(var c in nodes[i].classes) {
						if (nodes[i].classes[c] == 'constructor'
							|| nodes[i].classes[c] == 'destructor'
							|| nodes[i].classes[c] == 'operator') {
							nodeClasses += " operational";
						} else {
							nodeClasses += " " + nodes[i].classes[c];
						}
					}
				}
				elems.nodes.push({data: newNode, classes: nodeClasses});
			}
			
			var edges = data.edges;
			for(var i in edges) {
				var newEdge = {
					source: edges[i].source,
					target: edges[i].target,
					type: edges[i].type,
					description: edges[i].description
				};
				elems.edges.push({data: newEdge, classes: edges[i].type});
			}
		
		
            $('#cy').cytoscape({
				  layout: {
					name: 'cose',
					animate: false,
					refresh: 0,
					fit: true,
					padding: 80,
					boundingBox: { x1: 0, y1: 0, w: 200+30*data.nodes.length, h: 150+20*data.nodes.length },
					randomize: true,
					debug: false,
					nodeRepulsion: (data.class ? 100000000 : 1000000),
					nodeOverlap: 5,
					idealEdgeLength: (data.class ? 65 : 15),
					edgeElasticity: (data.class ? 100000 : 100),
					nestingFactor: 2,
					gravity: 200,
					numIter: 3000,
					initialTemp: 1000,
					coolingFactor: 0.99,
					minTemp: 1.0
				  },
				  
				  style: cytoscape.stylesheet()
					.selector('node')
					  .css({
						'shape': 'rectangle',
						'width': function(ele){
							var enlargementRatio = ele.hasClass('selected') ? 9 : 8;
							var text = ele.hasClass('selected') ? 'longName' : 'shortName';
							return 35+enlargementRatio*(ele.data(text).length);
							},
						'height': 30,
						'content': 'data(shortName)',
						'text-valign': 'center',
						'color': '#000',
						'border-width': 1,
						'border-color': '#000'
					  })
					.selector('node.namespace,node.main')
					  .css({
						'background-color': '#cccccc',
						'text-valign': 'top',
						'z-index': 0
					  })
					.selector('node.namespace')
					  .css({
						'border-width': 0,
						'text-halign': 'right'
					  })
					.selector('node.class')
					  .css({
						'background-color': '#D5D40D'
					  })
					.selector('node.struct,node.member')
					  .css({
						'background-color': '#3584EE'
					  })
					.selector('node.interface,node.connection')
					  .css({
						'background-color': '#46E322'
					  })
					.selector('node.method')
					  .css({
						'background-color': '#D5D40D'
					  })
					.selector('node.operational')
					  .css({
						'background-color': '#B5B40D'
					  })
					.selector('node.protected')
					  .css({
						'shape': 'roundrectangle'
					  })
					.selector('node.private')
					  .css({
						'shape': 'octagon'
					  })
					.selector('edge')
					  .css({
						'content': 'data(type)',
						'color': '#FFFFFF',
						'text-background-opacity': 1,
						'edge-text-rotation': 'autorotate',
						'width': 2
					  })
					.selector('edge.member')
					  .css({
						'line-color': '#EDA1ED',
						'text-background-color': '#EDA1ED',
						'source-arrow-shape': 'diamond',
						'source-arrow-color': '#EDA1ED'
					  })
					.selector('edge.inherits')
					  .css({
						'line-color': '#F5A45D',
						'text-background-color': '#F5A45D',
						'target-arrow-shape': 'triangle',
						'target-arrow-color': '#F5A45D'
					  })
					.selector('edge.derives,edge.override')
					  .css({
						'line-color': '#A5A40D',
						'text-background-color': '#A5A40D',
						'target-arrow-shape': 'triangle',
						'target-arrow-color': '#A5A40D'
					  })
					.selector('edge.parent,edge.call')
					  .css({
						'line-color': '#A5040D',
						'text-background-color': '#A5040D',
						'target-arrow-shape': 'triangle',
						'target-arrow-color': '#A5040D'
					  })
					.selector('edge.access')
					  .css({
						'line-color': '#05A4DD',
						'text-background-color': '#05A4DD',
						'target-arrow-shape': 'triangle',
						'target-arrow-color': '#05A4DD'
					  })
					.selector('.faded')
					  .css({
						'opacity': 0.4
					  })
					.selector('.faded:parent')
					  .css({
						'opacity': 1
					  })
					.selector('.selected')
					  .css({
						'font-weight': 'bold',
						'border-width': 3
					  })
					.selector('node.selected')
					  .css({
						'content': function( ele ){ return ele.data('type') + '\n' + ele.data('longName') },
						'text-wrap': 'wrap',
						'height': 40
					  })
					.selector('edge.selected')
					  .css({
						'content': function( ele ){ return ele.data('description') ? ele.data('type') + '\n' + ele.data('description') : ele.data('type') },
						'text-wrap': 'wrap',
						'edge-text-rotation': function( ele ){ return ele.data('description') ? 'none' : 'autorotate' }
					  }),
				  
				  elements: elems,
				  
				  ready: function(){
					window.cy = this;
					
					cy.elements().unselectify();
					
					cy.on('tap', 'node', function(e){
					  var node = e.cyTarget; 
					  if (node.hasClass('selected') && node.data('reference')) {
						var href = baseDir + node.data('reference') + '.json';
						try { // browser may block popups
							window.open(href);
						} catch(e){ // fall back on url change
							window.location.href = href; 
						} 
					  }
					  
					  cy.elements().removeClass('selected');
					  node.addClass('selected');
					  
					  var neighborhood = node.neighborhood().add(node).add(node.descendants());
					  neighborhood = neighborhood.add(neighborhood.edgesWith(neighborhood));
					  
					  cy.elements().addClass('faded');
					  neighborhood.removeClass('faded');
					  
					  cy.fit(neighborhood, 100);
					});
					
					cy.on('tap', 'edge', function(e){
					  var edge = e.cyTarget; 
					  cy.elements().removeClass('selected');
					  edge.addClass('selected');
					  
					  var elems = edge.connectedNodes().add(edge);
					  
					  cy.elements().addClass('faded');
					  elems.removeClass('faded');
					  
					  cy.fit(elems, 100);
					});
					
					cy.on('tap', function(e){
					  if( e.cyTarget === cy ){
						cy.elements().removeClass('selected');
						cy.elements().removeClass('faded');
						cy.fit([], 80);
					  }
					});
					
					var mouseButtonDown = false;
					cy.on('mousedown', function(event){
							mouseButtonDown = true;
						});
					cy.on('mouseup', function(event){
							mouseButtonDown = false;
						});
					
					cy.on('mousemove', function(event){
						var target = event ? event.cyTarget : undefined;
						if (!target || !('isNode' in target) || !target.isNode() && !target.isEdge()) return;
						if (mouseButtonDown) return;
						
						var sourceName = target.isNode() ? target.data("type") + "\n" + target.data("longName") : target.data("description");

                        $("#box").qtip({
                            content: {
                                text: sourceName
                            },
                            show: {
                                delay: 0,
                                event: false,
                                ready: true,
                                effect:false
                            },
                            position: {
                                my: 'bottom center',
                                at: 'top center',
                                target:[event.cyRenderedPosition.x,event.cyRenderedPosition.y - 25]
                            },
                            hide: {
                                fixed: true,
                                event: false,
                                inactive: 2000
                            },
                            style: {
                                classes: 'ui-tooltip-shadow ui-tooltip-youtube',
                                tip: { corner: false }
                            }
						});
					});
					
					
				  }
				});
				
				
        }
	};

}());