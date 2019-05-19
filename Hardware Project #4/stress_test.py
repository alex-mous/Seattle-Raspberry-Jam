#Code to put heavy load on CPU therefore raising the temperature

from multiprocessing import Pool
from multiprocessing import cpu_count
import math

def run(x): #Function to loop
    while True:
        math.sqrt(x) #Square root function

processes = cpu_count() #Get number of cores
pool = Pool(processes) #Create a pool of those cores
pool.map(run, range(processes)) #Run function run on each
