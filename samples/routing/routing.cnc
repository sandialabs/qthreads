[Object topology]; 			// key: iteration; value: matrix n*n - m[i,j] =1 if there is an edge between i and j
[Object tables];			// key: (iteration, subiteration, node); value: array or (nodeId, distance) pairs

<point nodeUpdateId>; 		// pairs (iteration, subiteration, nodeId)
<point nextIterationId>; 	// points with a signel dimension: iterationId
<nodeUpdateId>::(tableUpdate); 		// tableUpdate = another step towards convergence, builds new tables
<nextIterationId>::(networkUpdate); // iteration = new set of table updates after a topology change


[topology],[tables]->(tableUpdate)->[tables], <nodeUpdateId>, <nextIterationId>;
[topology]->(networkUpdate)->[topology],<nodeUpdateId>;
