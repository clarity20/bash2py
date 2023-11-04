#!/usr/bin/bash

f1()
{
    x=xxxxx
    echo in f1, x is $x

    f2()
    {
        y=yyyyy
        echo in f2, y is $y

        f3()
        {
            z=zzzzz
            echo in f3, z is $z
        }

        f3
    }

    f2
}

f1

