#include <iostream>
#include <vector>
#include <ctime>

#ifdef SEEK_SET
#undef SEEK_SET
#endif

#ifdef SEEK_CUR
#undef SEEK_CUR
#endif

#ifdef SEEK_END
#undef SEEK_END
#endif

#include <mpi.h>
using namespace std;

///////////////// GRUPO 6

int main(int argc, char *argv[])
{
	double t_inicio, t_fin;
	MPI_Init(&argc, &argv);
	int myid, numprocs;

	MPI_Comm_size(MPI_COMM_WORLD, &numprocs);
	MPI_Comm_rank(MPI_COMM_WORLD, &myid);
	MPI_Status status;

	srand(time(0));

	// la declaramos para todos porque tambien la necesitan los nodos
	int cant_colA_filB = std::atoi(argv[2]);
	// este sera un buffer que cada nodo tendra para saber a que posicion de fila y columna
	// pertenece su resultado en la matriz C
	int posicion[2];

	//// MASTER ///////

	if (myid == 0)
	{
		// instanciamos los tamaños elegidos para las matrices
		// esos parametros los pasamos cuando estamos ejecutando el programa
		int cant_filA = std::atoi(argv[1]);
		int cant_colB = std::atoi(argv[3]);

		// no es necesario comprobar que las matrices puedan multiplicarse
		// puesto que la variable cant_colA_filB ya nos asegura que sean compatibles
		// porque lo usamos en la declaracion de ambas

		// declaramos las matrices como arreglos de punteros a arreglos de enteros
		int **matrizA = new int *[cant_filA];
		// la usamos como traspuesta
		int **matrizB = new int *[cant_colB];
		int **matrizC = new int *[cant_filA];

		for (int i = 0; i < cant_filA; i++)
			matrizA[i] = new int[cant_colA_filB];
		for (int i = 0; i < cant_colB; i++)
			matrizB[i] = new int[cant_colA_filB];
		for (int i = 0; i < cant_filA; i++)
			matrizC[i] = new int[cant_colB];

		cout << "se pudieron crear las matrices " << endl;

		// rellenamos las matrices
		for (int j = 0; j < cant_filA; j++)
			for (int k = 0; k < cant_colA_filB; k++)
				matrizA[j][k] = rand() % 30;

		for (int j = 0; j < cant_colB; j++)
			for (int k = 0; k < cant_colA_filB; k++)
				matrizB[j][k] = rand() % 30;

		cout << "se rellenaron las matrices " << endl;

		// variables necesarias para mantener registro de donde estamos
		// en el proceso de calculo. Usadas para enviar los datos correctos
		// y verificar si ya termino todo el calculo
		// deben realizarse tamanoC cantidad de calculos
		int calculosRealizados = 0; //Indica si un nodo trabajador tiene que parar su ejecución
		// datosDelegados es para saber cuando dejar de enviar datos
		// y eso sera cuando datosDelegados == tamanoC
		int datosDelegados = 0;
		// cuando calculosRealizados == cant_filaA * cant_colB, entonces terminamos el producto de matrices
		// y actualizaremos el calculoTerminado a 1 (lo usamos como booleano)
		int calculoTerminado = 0;
		// filasAcompletadas y columnasBcompletadas sirven como indices para saber
		// que filas y columnas ir pasando a cada nodo a medida que van pidiendo nuevos datos
		int filasAcompletadas = 0;
		int columnasBcompletadas = 0;
		// buffers para pasar los vectores de datos. Son punteros a un vector de datos
		// y es asi porque MPI pide punteros en el primer parametro de MPI_Send
		int *bufferFila = new int[cant_colA_filB];
		int *bufferColumna = new int[cant_colA_filB];

		// hacemos un for inicial en el que enviamos una primer
		// tarea a cada nodo e iniciamos el ciclo de calculo
		cout << "****** INICIANDO PRODUCTO DE MATRICES ******" << endl;
		t_inicio = MPI_Wtime();		//Tiempos de ejecución total de producto
		for (int i = 1; i < numprocs; i++)
		{
			// informamos que el calculo aun no termina
			MPI_Send(&calculoTerminado, 1, MPI_INT, i, 4, MPI_COMM_WORLD);
			// enviamos primera fila de matrizA y primera columna de B
			bufferFila = matrizA[filasAcompletadas];
			bufferColumna = matrizB[columnasBcompletadas];
			// tag 1 para enviar y recibir fila y tag 2 para columna
			MPI_Send(bufferFila, cant_colA_filB, MPI_INT, i, 1, MPI_COMM_WORLD);
			MPI_Send(bufferColumna, cant_colA_filB, MPI_INT, i, 2, MPI_COMM_WORLD);
			// y tambien debemos enviar cual es la posicion en la matrizC del producto resultante
			posicion[0] = filasAcompletadas;
			posicion[1] = columnasBcompletadas;
			// tag 5 para enviar y recibir posicion
			MPI_Send(posicion, 2, MPI_INT, i, 5, MPI_COMM_WORLD);

			columnasBcompletadas++;
			datosDelegados++;
			// acomodamos los indices
			if (columnasBcompletadas == cant_colB)
			{
				columnasBcompletadas = 0;
				filasAcompletadas++;
			}
		}

		// en esta variable almacenamos el rango del nodo que ha finalizado el proceso
		// y asi podemos volver a enviarle un nuevo calculo para que realice
		int rankNodoEmisor;

		do
		{
			// recibimos la posicion del resultado entrante
			MPI_Recv(&posicion, 2, MPI_INT, MPI_ANY_SOURCE, 5, MPI_COMM_WORLD, &status);
			// obtenemos el rank del nodo que envio el resultado del producto
			rankNodoEmisor = status.MPI_SOURCE;
			// almacenamos el resultado en la posicion correspondiente
			// asegurandonos de que el mensaje sea del mismo nodo que envio la posicion
			// porque si no guardariamos cualquier resultado si es que justo otro nodo
			// envia mensaje
			// tag 3 para enviar y recibir producto
			MPI_Recv(&matrizC[posicion[0]][posicion[1]], 1, MPI_INT, rankNodoEmisor, 3, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
			// aumentamos la cantidad de calculosRealizados para saber si debemos frenar a los demas nodos
			calculosRealizados++;

			//  el do while loop es un poco al pedo, porque siempre saldremos con esta condicion, antes de hacer otro send que este de mas
			//  aumentamos en 1 porque calculosRealizados se usa para los indices, y comienza en 0
			if (datosDelegados == cant_filA * cant_colB)
			{
				// ya entregamos todos los datos. Ahora debemos asegurarnos de haber
				// recibido todos los resultados
				calculoTerminado = 1;
				while (calculosRealizados < cant_filA * cant_colB)
				{
					MPI_Recv(&posicion, 2, MPI_INT, MPI_ANY_SOURCE, 5, MPI_COMM_WORLD, &status);
					rankNodoEmisor = status.MPI_SOURCE;
					MPI_Recv(&matrizC[posicion[0]][posicion[1]], 1, MPI_INT, rankNodoEmisor, 3, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
					calculosRealizados++;
				}

				for (int t = 1; t < numprocs; t++)
				{
					// enviamos la señal de apagado que se quedo esperando el nodo trabajador
					// tag 4 para señal de terminado
					MPI_Send(&calculoTerminado, 1, MPI_INT, t, 4, MPI_COMM_WORLD);
				}

				// y listo aca ya deberiamos tener todos los resultados
				t_fin = MPI_Wtime();

				// SE DEBE DESCOMENTAR ESTA PARTE PARA MOSTRAR LOS RESULTADOS PARA PANTALLA
				// SI LAS MATRICES SON GRANDES VA A ESTAR MEDIA HORA ESTO
				cout << "*********Procesamiento terminado*********" << endl;
				// cout << " LOS RESULTADOS SON " << endl;

				// cout << " La matriz A era: " << endl;
				// for (int i = 0; i < cant_filA; i++)
				// {
				// 	for (int j = 0; j < cant_colA_filB; j++)
				// 	{
				// 		cout << matrizA[i][j] << " ";
				// 	}
				// 	cout << endl;
				// }

				// cout << endl
				// 	 << " La matriz B era: " << endl;
				// for (int i = 0; i < cant_colA_filB; i++)
				// {
				// 	for (int j = 0; j < cant_colB; j++)
				// 	{
				// 		cout << matrizB[j][i] << " ";
				// 	}
				// 	cout << endl;
				// }

				// cout << endl
				// 	 << " La matriz C fue: " << endl;
				// for (int i = 0; i < cant_filA; i++)
				// {
				// 	for (int j = 0; j < cant_colB; j++)
				// 	{
				// 		cout << matrizC[i][j] << " ";
				// 	}
				// 	cout << endl;
				// }

				break;
				/////////////// SALE DEL CICLO DESPUES DE MOSTRAR LAS MATRICES //////////
			}

			// acomodamos los indices de filas y columnas en caso de que hayamos terminado toda una fila de A para la misma columna de B
			// si terminamos de procesar todas las columnas de matrizB para una misma fila de matrizA, entonces reiniciamos las columnas de B a 0
			// y aumentamos las filas de A en 1
			if (columnasBcompletadas == cant_colB)
			{
				columnasBcompletadas = 0;
				filasAcompletadas++;
			}

			bufferFila = matrizA[filasAcompletadas];
			bufferColumna = matrizB[columnasBcompletadas];
			posicion[0] = filasAcompletadas;
			posicion[1] = columnasBcompletadas;

			MPI_Send(&calculoTerminado, 1, MPI_INT, rankNodoEmisor, 4, MPI_COMM_WORLD);
			MPI_Send(bufferFila, cant_colA_filB, MPI_INT, rankNodoEmisor, 1, MPI_COMM_WORLD);
			MPI_Send(bufferColumna, cant_colA_filB, MPI_INT, rankNodoEmisor, 2, MPI_COMM_WORLD);
			MPI_Send(posicion, 2, MPI_INT, rankNodoEmisor, 5, MPI_COMM_WORLD);
			// como delegamos un nuevo conjunto de fila y columna, aumentamos la variable
			datosDelegados++;
			// incrementamos las columnas completadas
			columnasBcompletadas++;

			// mostramos la cantidad de calculos realizados de vez en cuando para ver como va el
			// proceso
			//cout << "Calculos realizados: " << calculosRealizados << endl;
		} while (true);

		double tiempo_ejecucion = t_fin - t_inicio;
		cout << "Tiempo total de procesamiento: " << tiempo_ejecucion << endl;
		cout << "master termino de ejecutarse " << endl;
	}

	///// NODOS TRABAJADORES /////

	if (myid != 0)
	{
		// donde almacenamos el resultado
		int producto;
		// vectores donde vamos a recibir la fila y columna a multiplicar
		int *filaA = new int[cant_colA_filB];
		int *colB = new int[cant_colA_filB];

		// bandera para frenar la ejecucion del nodo si es que terminamos
		// todos los calculos
		int calculoTerminado = 0;

		// comenzamos a medir el tiempo total de ejecucion de los nodos
		double t_inicio_nodo = MPI_Wtime();
		// e iniciamos el tiempo de procesamiento total en 0
		double t_procesamiento = 0;
		// cada nodo entra en un loop hasta que se cumpla la misma condicion que en el master
		// que todos los calculos se hayan realizado
		do
		{
			// hacemos esto al principio, y vemos que si calculoTerminado es true entonces salimos del bucle y termina
			MPI_Recv(&calculoTerminado, 1, MPI_INT, 0, 4, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
			// verificamos si recibio la señal de apagado
			if (calculoTerminado == 1)
			{
				double t_fin_nodo = MPI_Wtime();
				double t_total_nodo = t_fin_nodo - t_inicio_nodo;
				cout << "Nodo " << myid << " terminando ejecucion" << endl;
				cout << "Tiempo de ejecucion de " << myid << " fue: " << t_total_nodo << endl;
				cout << "Tiempo de procesamiento de " << myid << " fue: " << t_procesamiento << endl;
				break;
			}
			producto = 0;
			// los tag 1 y 2 no son necesarios, pero los usamos para indicar que estamos recibiendo fila en 1 y columna en 2
			MPI_Recv(filaA, cant_colA_filB, MPI_INT, 0, 1, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
			MPI_Recv(colB, cant_colA_filB, MPI_INT, 0, 2, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
			// recibe la posicion a la que pertenece, y luego la vuelve a enviar. Este dato es crucial
			// enviarlo y reenviarlo porque asi permitimos que el programa sea practicamente asincrono
			// y no debe esperar a la resolucion de cada nodo para volver a enviar una tarea a otro
			MPI_Recv(posicion, 2, MPI_INT, 0, 5, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
			// calculamos el producto de los vectores, y medimos el tiempo para sumar al total de procesamiento
			double t_inicio_calculo = MPI_Wtime();
			for (int j = 0; j < cant_colA_filB; j++)
			{
				producto += filaA[j] * colB[j];
			}
			double t_fin_calculo = MPI_Wtime();
			//  cout<<"Se tardó en procesar una fila * columna "<<t_fin_calculo - t_inicio_calculo<< "el nodo" << myid <<endl;
			t_procesamiento += t_fin_calculo - t_inicio_calculo;
			//  enviamos primero la posicion, y luego el resultado. Tag 5 es para informar posicion
			MPI_Send(posicion, 2, MPI_INT, 0, 5, MPI_COMM_WORLD);
			// el tag de 3 lo usamos para indicar que enviamos el producto. Utilizamos el mismo 3 en el receive del master
			MPI_Send(&producto, 1, MPI_INT, 0, 3, MPI_COMM_WORLD);
		} while (true);
	}

	MPI_Finalize();

	return 0;
}
