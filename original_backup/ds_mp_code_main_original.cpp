#include <iostream>
#include <string>
#include <limits>
using namespace std;

#define MAX 100

// Structure to store vehicle details
struct Vehicle
{
    string vehicleID;
    int batteryLevel;
    float requiredEnergy;
    string connectorType;
};

// Structure to store charging station details
struct Station
{
    int stationID;
    float chargingSpeed;
    string connectorType;
    bool available;
    string currentVehicle;
};

// Arrays
Vehicle vehicles[MAX];
Station stations[MAX];

int vehicleCount = 0;
int stationCount = 0;

// Queue implementation
Vehicle waitingQueue[MAX];

int front = 0;
int rear = -1;
int queueCount = 0;


// INPUT VALIDATION FUNCTIONS

bool readInt(string message, int &value)
{
    cout << message;

    if (cin >> value)
        return true;

    if (cin.eof())
        return false;

    cin.clear();
    cin.ignore(numeric_limits<streamsize>::max(), '\n');

    cout << "Invalid input!\n";

    return false;
}


bool readFloat(string message, float &value)
{
    cout << message;

    if (cin >> value)
        return true;

    if (cin.eof())
        return false;

    cin.clear();
    cin.ignore(numeric_limits<streamsize>::max(), '\n');

    cout << "Invalid input!\n";

    return false;
}


bool readString(string message, string &value)
{
    cout << message;

    if (cin >> value)
        return true;

    return false;
}


// FUNCTION TO ADD VEHICLE TO QUEUE

bool enqueue(Vehicle v)
{
    if (queueCount == MAX)
    {
        cout << "\nWaiting queue is full!\n";
        return false;
    }

    if (rear == MAX - 1)
    {
        for (int i = 0; i < queueCount; i++)
        {
            waitingQueue[i] = waitingQueue[front + i];
        }

        front = 0;
        rear = queueCount - 1;
    }

    rear++;

    waitingQueue[rear] = v;

    queueCount++;

    cout << "\nVehicle added to waiting queue.\n";

    return true;
}


// FUNCTION TO REMOVE VEHICLE FROM QUEUE

void removeVehicle(int position)
{
    for (int i = position; i < rear; i++)
    {
        waitingQueue[i] = waitingQueue[i + 1];
    }

    rear--;

    queueCount--;

    if (queueCount == 0)
    {
        front = 0;
        rear = -1;
    }
}


// BUBBLE SORT

void sortStations()
{
    for (int i = 0; i < stationCount - 1; i++)
    {
        for (int j = 0; j < stationCount - i - 1; j++)
        {
            if (stations[j].chargingSpeed <
                stations[j + 1].chargingSpeed)
            {
                Station temp = stations[j];

                stations[j] = stations[j + 1];

                stations[j + 1] = temp;
            }
        }
    }
}


// SEQUENTIAL SEARCH

int searchStation(Vehicle v)
{
    for (int i = 0; i < stationCount; i++)
    {
        if (stations[i].available == true &&
            stations[i].connectorType == v.connectorType)
        {
            return i;
        }
    }

    return -1;
}


// FUNCTION TO ASSIGN CHARGING SLOT

bool assignSlot(Vehicle v)
{
    int index = searchStation(v);

    if (index == -1)
    {
        return false;
    }

    stations[index].available = false;

    stations[index].currentVehicle = v.vehicleID;

    float chargingTime =
        (v.requiredEnergy / stations[index].chargingSpeed) * 60;

    cout << "\n========================================\n";

    cout << "   CHARGING SLOT SUCCESSFULLY ASSIGNED\n";

    cout << "========================================\n";

    cout << "Vehicle Number   : " << v.vehicleID << endl;

    cout << "Battery Level    : " << v.batteryLevel << "%" << endl;

    cout << "Station ID       : " << stations[index].stationID << endl;

    cout << "Charging Speed   : "
         << stations[index].chargingSpeed << " kW" << endl;

    cout << "Connector Type   : "
         << stations[index].connectorType << endl;

    cout << "Estimated Time   : "
         << chargingTime << " minutes" << endl;

    cout << "========================================\n";

    return true;
}


// FUNCTION TO PROCESS WAITING QUEUE

void processQueue()
{
    int i = 0;

    while (i < queueCount)
    {
        Vehicle v = waitingQueue[i];

        if (assignSlot(v))
        {
            removeVehicle(i);
        }
        else
        {
            i++;
        }
    }

    if (queueCount == 0)
    {
        cout << "\nAll waiting vehicles have been processed.\n";
    }
    else
    {
        cout << "\nSome vehicles are still waiting for compatible stations.\n";
    }
}


// FUNCTION TO ADD NEW VEHICLE

void addVehicle()
{
    if (vehicleCount == MAX)
    {
        cout << "\nVehicle storage is full!\n";
        return;
    }

    Vehicle v;

    if (!readString("\nEnter Vehicle Registration Number: ", v.vehicleID))
        return;

    // CHECK DUPLICATE VEHICLE NUMBER

    for (int i = 0; i < vehicleCount; i++)
    {
        if (vehicles[i].vehicleID == v.vehicleID)
        {
            cout << "\nVehicle already registered!\n";
            return;
        }
    }

    if (!readInt("Enter Battery Level (%): ", v.batteryLevel))
        return;

    if (!readFloat("Enter Required Energy (kWh): ", v.requiredEnergy))
        return;

    if (!readString("Enter Connector Type (CCS/CHAdeMO/Type2): ",
                    v.connectorType))
        return;

    if (v.batteryLevel < 0 || v.batteryLevel > 100 ||
        v.requiredEnergy <= 0)
    {
        cout << "\nInvalid vehicle information!\n";
        return;
    }

    // Maintain existing waiting order

    if (queueCount > 0)
    {
        if (enqueue(v))
        {
            vehicles[vehicleCount] = v;
            vehicleCount++;

            processQueue();
        }

        return;
    }

    // Try assigning a station directly

    if (assignSlot(v))
    {
        vehicles[vehicleCount] = v;
        vehicleCount++;
    }
    else
    {
        if (enqueue(v))
        {
            vehicles[vehicleCount] = v;
            vehicleCount++;
        }

        cout << "\nNo compatible charging station is currently available.\n";
    }
}


// FUNCTION TO ADD CHARGING STATIONS

void addStations()
{
    if (!readInt("\nEnter number of charging stations: ", stationCount))
    {
        stationCount = 0;
        return;
    }

    if (stationCount <= 0 || stationCount > MAX)
    {
        cout << "\nInvalid number of stations! Enter between 1 and 100.\n";

        stationCount = 0;

        return;
    }

    for (int i = 0; i < stationCount; i++)
    {
        cout << "\n========== STATION " << i + 1 << " ==========\n";

        if (!readInt("Enter Station ID: ", stations[i].stationID))
        {
            stationCount = 0;
            return;
        }

        // CHECK DUPLICATE STATION ID

        for (int j = 0; j < i; j++)
        {
            if (stations[j].stationID == stations[i].stationID)
            {
                cout << "\nDuplicate Station ID! Enter a different ID.\n";

                stationCount = 0;

                return;
            }
        }

        if (!readFloat("Enter Charging Speed (kW): ",
                       stations[i].chargingSpeed))
        {
            stationCount = 0;
            return;
        }

        if (stations[i].chargingSpeed <= 0)
        {
            cout << "\nCharging speed must be positive!\n";

            stationCount = 0;

            return;
        }

        if (!readString("Enter Connector Type (CCS/CHAdeMO/Type2): ",
                        stations[i].connectorType))
        {
            stationCount = 0;
            return;
        }

        stations[i].available = true;

        stations[i].currentVehicle = "";
    }

    sortStations();

    cout << "\nCharging stations registered successfully!\n";

    cout << "Stations arranged according to charging speed.\n";
}


// FUNCTION TO COMPLETE CHARGING

void completeCharging()
{
    int id;

    if (!readInt("\nEnter Station ID: ", id))
        return;

    for (int i = 0; i < stationCount; i++)
    {
        if (stations[i].stationID == id)
        {
            if (stations[i].available == false)
            {
                cout << "\nCharging completed for Vehicle Number: "
                     << stations[i].currentVehicle << endl;

                stations[i].available = true;

                stations[i].currentVehicle = "";

                cout << "\nCharging station is now available.\n";

                processQueue();

                return;
            }

            cout << "\nThis station is already available.\n";

            return;
        }
    }

    cout << "\nStation not found!\n";
}


// FUNCTION TO DISPLAY CHARGING STATIONS

void displayStations()
{
    cout << "\n========== CHARGING STATIONS ==========\n";

    for (int i = 0; i < stationCount; i++)
    {
        cout << "\nStation ID: " << stations[i].stationID;

        cout << "\nCharging Speed: "
             << stations[i].chargingSpeed << " kW";

        cout << "\nConnector Type: "
             << stations[i].connectorType;

        if (stations[i].available)
        {
            cout << "\nStatus: AVAILABLE";
        }
        else
        {
            cout << "\nStatus: OCCUPIED";

            cout << "\nVehicle Number: "
                 << stations[i].currentVehicle;
        }

        cout << "\n---------------------------------------\n";
    }
}


// FUNCTION TO DISPLAY WAITING QUEUE

void displayQueue()
{
    cout << "\n========== WAITING VEHICLES ==========\n";

    if (queueCount == 0)
    {
        cout << "No vehicles waiting.\n";
        return;
    }

    for (int i = 0; i < queueCount; i++)
    {
        cout << "\nVehicle Number: "
             << waitingQueue[front + i].vehicleID;

        cout << "\nBattery Level: "
             << waitingQueue[front + i].batteryLevel << "%";

        cout << "\nConnector Type: "
             << waitingQueue[front + i].connectorType;

        cout << "\n-----------------------------\n";
    }

    cout << "\nTotal Waiting Vehicles: "
         << queueCount << endl;
}


// MAIN FUNCTION

int main()
{
    int choice = 0;

    cout << "\n============================================\n";

    cout << "       EV CHARGING SLOT ASSIGNMENT SYSTEM\n";

    cout << "============================================\n";

    // Register charging stations

    addStations();

    if (stationCount == 0)
    {
        cout << "\nUnable to initialize charging stations.\n";

        return 0;
    }

    do
    {
        cout << "\n\n============== MAIN MENU ==============\n";

        cout << "1. Register New Vehicle\n";

        cout << "2. Complete Charging\n";

        cout << "3. Display Charging Stations\n";

        cout << "4. Display Waiting Queue\n";

        cout << "5. Process Waiting Queue\n";

        cout << "6. Exit\n";

        cout << "=======================================\n";

        if (!readInt("Enter Your Choice: ", choice))
        {
            cout << "\nInvalid input or input ended. Exiting program.\n";

            break;
        }

        switch (choice)
        {
            case 1:
                addVehicle();
                break;

            case 2:
                completeCharging();
                break;

            case 3:
                displayStations();
                break;

            case 4:
                displayQueue();
                break;

            case 5:
                processQueue();
                break;

            case 6:
                cout << "\nThank you for using EV Charging System!\n";
                break;

            default:
                cout << "\nInvalid Choice! Enter a number between 1 and 6.\n";
        }

    } while (choice != 6);

    return 0;
}